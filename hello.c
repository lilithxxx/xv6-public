#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  // 1: use syscall readcount
  // for (int i = 0; i < 10; ++i) {
  //   int readcount = getreadcount();
  //   printf(1, "readcount: %d\n", readcount);
  //   int buf[1];
  //   read(0, &buf, 0);
  // }

  // 2: null pointer deref gets error
  // char *p = 0;
  // printf(1, "%x\n", (unsigned int)*p);

  char *p = malloc(4096);  // Allocate a page
  printf(1, "Initial value: %c\n", *p);
  *p = 'A';
  printf(1, "Before mprotect: %c\n", *p);

  mprotect((void*)((uint)p & ~0xFFF), 1);  // Page-align address
  // printf(1, "After mprotect, trying write...\n");
  // *p = 'B';  // Should crash here

  munprotect((void*)((uint)p & ~0xFFF), 1);
  // *p = 'B';
  // printf(1, "After munprotect: %c\n", *p);

  printf(1, "\nForking child (should crash)...\n");
  int pid = fork();
  if(pid == 0) {
    *p = 'B';  // Child tries to write - should crash
    printf(1, "Child: %c\n", *p);
    exit();
  }
  wait();
  exit();
}
