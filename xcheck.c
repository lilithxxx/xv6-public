#define main mkfs_main
#include "mkfs.c"
#undef main

#define assert_msg(cond, msg)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s\n", (msg));                                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

int readbmap(int db) {
  char buf[BSIZE];
  rsect(sb.bmapstart, buf);
  return (buf[db / 8] >> db % 8) & 0x1;
}

uint get_block(struct dinode *din, uint i) {
  uint addr;
  if (i <= NDIRECT)
    addr = din->addrs[i];
  else if (din->addrs[NDIRECT] == 0)
    addr = 0;
  else {
    uint ind[NINDIRECT];
    rsect(din->addrs[NDIRECT], ind);
    addr = ind[i - NDIRECT - 1]; // -1 for the indirect block itself
  }
  if (addr != 0) {
    assert_msg(sb.size - sb.nblocks <= addr && addr < sb.size,
               "ERROR: bad block address in inode");
    assert_msg(readbmap(addr),
               "ERROR: address used by inode but marked free in bitmap");
  }
  return addr;
}

int dir_exist(char *name, uint rootinum) {
  struct dinode din;
  rinode(rootinum, &din);
  if (din.type != T_DIR)
    return 0;

  for (int i = 0; i < NDIRECT + NINDIRECT + 1; i++) {
    uint db = get_block(&din, i);
    if (db == 0 || i == NDIRECT)
      continue;
    char buf[BSIZE];
    rsect(db, buf);
    struct dirent *dirs = (struct dirent *)buf;
    for (int j = 0; j < BSIZE / sizeof(struct dirent); j++) {
      if (strcmp(dirs[j].name, "..") == 0 || strcmp(dirs[j].name, ".") == 0)
        continue;

      if (strcmp(dirs[j].name, name) == 0)
        return dirs[j].inum;
      int exists = dir_exist(name, dirs[j].inum);
      if (exists)
        return exists;
    }
  }

  return 0;
}

int num_dirs(int inum) {
  struct dinode din;
  rinode(inum, &din);
  if (din.type != T_DIR)
    return 0;
  int res = 1;

  for (int i = 0; i < NDIRECT + NINDIRECT + 1; i++) {
    uint db = get_block(&din, i);
    if (db == 0 || i == NDIRECT)
      continue;
    char buf[BSIZE];
    rsect(db, buf);
    struct dirent *dirs = (struct dirent *)buf;
    for (int j = 0; j < BSIZE / sizeof(struct dirent); j++) {
      if (strcmp(dirs[j].name, "..") != 0 && strcmp(dirs[j].name, ".") != 0)
        res += num_dirs(dirs[j].inum);
    }
  }
  return res;
}

// is inum_exp in inum_in?
int inum_present_in_dir(uint inum_exp, uint inum_in) {
  struct dinode din;
  rinode(inum_in, &din);
  for (int i = 0; i < NDIRECT + NINDIRECT + 1; i++) {
    uint db = get_block(&din, i);
    if (db == 0 || i == NDIRECT)
      continue;
    char buf[BSIZE];
    rsect(db, buf);
    struct dirent *dirs = (struct dirent *)buf;
    for (int j = 0; j < BSIZE / sizeof(struct dirent); j++) {
      if (dirs[j].inum == inum_exp)
        return 1;
    }
  }
  return 0;
}

// gets the first free block from the bitmap
// also writes to the bitmap and zeroes the block
uint get_and_write_free_block() {
  char buf[BSIZE];
  rsect(sb.bmapstart, buf);
  uint freeblock = 0;
  for (int i = 0; i < BSIZE; i++) {
    unsigned char t = buf[i];
    int pos = 0;
    for (int j = 0; j < 8; j++) {
      if (freeblock == sb.size)
        goto err;
      if ((t & 0x1) == 0) {
        buf[i] = buf[i] | (0x1 << pos);
        wsect(sb.bmapstart, buf);
        wsect(freeblock, zeroes);
        return freeblock;
      }
      t >>= 1;
      freeblock++;
      pos++;
    }
  }

err:
  fprintf(stderr, "%s\n", ("ERROR: no datablock free"));
  exit(1);
}

void iappend2(uint inum, void *xp, int n) {
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  while (n > 0) {
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        din.addrs[fbn] = get_and_write_free_block();
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[NDIRECT]) == 0) {
        din.addrs[NDIRECT] = get_and_write_free_block();
      }
      rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = xint(get_and_write_free_block());
        wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}

int main(int argc, char *argv[]) {
  char buf[BSIZE];
  struct dinode din;
  int total_db = 0;
  int total_dirs = 0;
  char seen[FSSIZE];
  memset(seen, 0, sizeof(seen));
  int inode_counter[NINODES];
  memset(inode_counter, 0, sizeof(inode_counter));

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  assert_msg(argc >= 2, "Usage: mkfs fs.img [-r]\n");

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR, 0666);
  assert_msg(fsfd >= 0, argv[1]);

  // Check for -r flag (repair)
  int repair = argc == 3 && strcmp(argv[2], "-r") == 0;

  // Read superblock
  rsect(1, buf);
  memmove(&sb, buf, sizeof(sb));

  // Get freeinode for ialloc to work
  for (freeinode = 1; freeinode < sb.ninodes; freeinode++) {
    struct dinode temp;
    rinode(freeinode, &temp);
    if (temp.type == 0)
      break;
  }

  // Create the lost and found directory
  uint lostdir_inum = 0;
  if (repair) {
    lostdir_inum = dir_exist("lost_found", ROOTINO);
    if (lostdir_inum == 0) {
      if (freeinode == sb.ninodes) {
        fprintf(stderr, "%s\n", ("ERROR: no inode free"));
        exit(1);
      }
      lostdir_inum = ialloc(T_DIR);
      struct dirent de;
      bzero(&de, sizeof(de));
      de.inum = xshort(lostdir_inum);
      strcpy(de.name, ".");
      iappend2(lostdir_inum, &de, sizeof(de));

      bzero(&de, sizeof(de));
      de.inum = xshort(ROOTINO);
      strcpy(de.name, "..");
      iappend2(lostdir_inum, &de, sizeof(de));

      bzero(&de, sizeof(de));
      de.inum = lostdir_inum;
      strcpy(de.name, "lost_found");
      iappend2(ROOTINO, &de, sizeof(de));
    }
  }

  // 1. Check for rootino and directory consistency
  rinode(ROOTINO, &din);
  assert_msg(din.type == T_DIR, "ERROR: root directory does not exist");
  int root_parent_found = 0;

  int currinode = -1;

  for (int k = sb.inodestart; k < sb.bmapstart; k++) {
    rsect(k, buf);
    struct dinode *inodes = (struct dinode *)buf;
    for (int k2 = 0; k2 < IPB; k2++) {
      din = inodes[k2];
      currinode++;
      if (din.type != T_DIR)
        continue;

      total_dirs++;
      int currdir_present = 0;
      int parentdir_present = 0;

      for (int i = 0; i < NDIRECT + NINDIRECT + 1; i++) {
        uint db = get_block(&din, i);
        if (db == 0 || i == NDIRECT)
          continue;
        char buf2[BSIZE];
        rsect(db, buf2);
        struct dirent *dirs = (struct dirent *)buf2;
        for (int j = 0; j < BSIZE / sizeof(struct dirent); j++) {
          if (dirs[j].inum == 0)
            continue; // unallocted
          assert_msg(dirs[j].inum > 0 && dirs[j].inum < sb.ninodes,
                     "ERROR: inum out of range");
          if (currinode == ROOTINO && strcmp(dirs[j].name, "..") == 0 &&
              dirs[j].inum == ROOTINO)
            root_parent_found = 1;
          struct dinode din2;
          rinode(dirs[j].inum, &din2);
          assert_msg(din2.type != 0,
                     "ERROR: inode referred to in directory but marked free");
          if (strcmp(dirs[j].name, ".") != 0 && strcmp(dirs[j].name, "..") != 0)
            inode_counter[dirs[j].inum]++;

          if (strcmp(dirs[j].name, "..") == 0 && currinode != ROOTINO)
            assert_msg(inum_present_in_dir(currinode, dirs[j].inum),
                       "ERROR: parent directory mismatch");

          if (strcmp(dirs[j].name, "..") == 0)
            parentdir_present = 1;
          if (strcmp(dirs[j].name, ".") == 0)
            currdir_present = 1;
        }
      }
      assert_msg(parentdir_present == 1, "ERROR: parent directory not found");
      assert_msg(currdir_present == 1, "ERROR: current directory not found");
    }
  }
  assert_msg(root_parent_found == 1, "ERROR: root directory does not exist");

  // 2. Check if entries in bitmap till inodes are all allocated (data blocks
  // can only be unallocated)
  for (int i = 0; i < sb.size - sb.nblocks; i++)
    assert_msg(readbmap(i),
               "ERROR: all metadata blocks should be allocated in bitmap");

  currinode = -1;

  // 3. Check if each inode is either unallocated or a valid type
  for (int i = sb.inodestart; i < sb.bmapstart; i++) {
    rsect(i, buf);
    struct dinode *inodes = (struct dinode *)buf;
    for (int j = 0; j < IPB; j++) {
      assert_msg(inodes[j].type == 0 || inodes[j].type == T_DIR ||
                     inodes[j].type == T_FILE || inodes[j].type == T_DEV,
                 "ERROR: bad inode");
      currinode++;
      if (inodes[j].type == 0)
        continue;

      // Handle the datablocks of lost_found dir seperately at end
      // because this can get new datablocks
      if (currinode == lostdir_inum)
        continue;

      if (inodes[j].type == T_FILE) {
        if (repair && inode_counter[currinode] == 0) {
          struct dirent lostdirent;
          bzero(&lostdirent, sizeof(lostdirent));
          lostdirent.inum = currinode;
          sprintf(lostdirent.name, "%d", currinode);
          iappend2(lostdir_inum, &lostdirent, sizeof(lostdirent));
          inode_counter[currinode] = 1;
          inodes[j].nlink = 1;
          winode(currinode, &inodes[j]);
		  printf("SUCCESS: Repaired the file!\n");
        } else
          assert_msg(inode_counter[currinode] > 0,
                     "ERROR: inode marked use but not found in a directory");

        assert_msg(inode_counter[currinode] == inodes[j].nlink,
                   "ERROR: bad reference count for file");
      }

      if (inodes[j].type == T_DIR && currinode != ROOTINO) {
        assert_msg(inode_counter[currinode] == 1,
                   "ERROR: directory appears more than once in file system");
      } else if (inodes[j].type == T_DIR && currinode == ROOTINO) {
        assert_msg(inode_counter[currinode] == 0,
                   "ERROR: directory appears more than once in file system");
      }

      for (int k = 0; k < NDIRECT + NINDIRECT + 1; k++) {
        uint db = get_block(&inodes[j], k);
        if (db == 0)
          continue;
        assert_msg(!seen[db], "ERROR: block address used more than once");
        seen[db] = 1;
        total_db++;
      }
    }
  }

  // Handle the lost_found_dir now
  struct dinode lost_found_dir;
  rinode(lostdir_inum, &lost_found_dir);

  for (int i = 0; i < NDIRECT + NINDIRECT + 1; i++) {
    uint db = get_block(&lost_found_dir, i);
    if (db == 0)
      continue;
    assert_msg(!seen[db], "ERROR: block address used more than once");
    seen[db] = 1;
    total_db++;
  }

  // 4. Check if actual total datablocks allocated is same as marked in bitmap
  int exp_db = 0;
  rsect(sb.bmapstart, buf);
  for (int i = 0; i < BSIZE; i++) {
    unsigned char t = buf[i];
    while (t) {
      if (t & 0x1)
        exp_db++;
      t >>= 1;
    }
  }
  int nmeta = sb.size - sb.nblocks;
  assert_msg(exp_db == total_db + nmeta,
             "ERROR: bitmap marks block in use but it is not in use");

  int num_dirs_from_root = num_dirs(ROOTINO);
  assert_msg(num_dirs_from_root == total_dirs,
             "ERROR: inaccessible directory exists.");

  printf("SUCCESS: FS image not corrupted!\n");
  return 0;
}
