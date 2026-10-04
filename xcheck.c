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

int main(int argc, char *argv[]) {
  char buf[BSIZE];
  struct dinode din;
  int total_db = 0;
  char seen[FSSIZE];
  memset(seen, 0, sizeof(seen));
  int inode_counter[NINODES];
  memset(inode_counter, 0, sizeof(inode_counter));

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  assert_msg(argc >= 2, "Usage: mkfs fs.img files...\n");

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDONLY, 0666);
  assert_msg(fsfd >= 0, argv[1]);

  // Read superblock
  rsect(1, buf);
  memmove(&sb, buf, sizeof(sb));

  // 1. Check for rootino consistency
  // a. Check if .. is in its direct pointers and handle the root case
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
        }
      }
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

      if (inodes[j].type == T_FILE) {
        assert_msg(inode_counter[currinode] > 0,
                   "ERROR: inode marked use but not found in a directory");
        assert_msg(inode_counter[currinode] == inodes[j].nlink,
                   "ERROR: bad reference count for file");
      }

      if (inodes[j].type == T_DIR && currinode != ROOTINO) {
        assert_msg(inode_counter[currinode] == 1,
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

  printf("SUCCESS: FS image not corrupted!\n");
  return 0;
}
