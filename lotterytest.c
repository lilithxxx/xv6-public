// Lottery scheduler test - snapshots ticks while processes are running

#include "types.h"
#include "user.h"
#include "pstat.h"

int
main(void)
{
  struct pstat ps;
  int i, j;
  int pids[3];
  int tickets[3] = {1, 10, 100};

  printf(1, "Lottery Scheduler Test\n");
  printf(1, "Starting 3 processes: 1, 10, 100 tickets\n\n");

  // Fork 3 children that run infinite loops
  for(i = 0; i < 3; i++){
    pids[i] = fork();
    if(pids[i] < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pids[i] == 0){
      // Child: set tickets and loop forever
      settickets(tickets[i]);
      for(;;){
        // Burn CPU
        int x = 0;
        int k;
        for(k = 0; k < 1000000; k++)
          x += k;
      }
    }
  }

  // Parent: sleep a bit, then snapshot 3 times
  for(i = 1; i <= 3; i++){
    // Let children run for a bit
    sleep(300);  // Sleep for ~3 second

    if(getpinfo(&ps) < 0){
      printf(1, "getpinfo failed\n");
      exit();
    }

    printf(1, "=== Snapshot %d ===\n", i);
    printf(1, "PID\tTickets\tTicks\n");

    // Print only our 3 children
    for(j = 0; j < 3; j++){
      int k;
      for(k = 0; k < NPROC; k++){
        if(ps.pid[k] == pids[j] && ps.inuse[k]){
          printf(1, "%d\t%d\t%d\n", ps.pid[k], ps.tickets[k], ps.ticks[k]);
          break;
        }
      }
    }
    printf(1, "\n");
  }

  // Kill children
  for(i = 0; i < 3; i++){
    kill(pids[i]);
  }

  // Wait for them to die
  for(i = 0; i < 3; i++){
    wait();
  }
  exit();
}
