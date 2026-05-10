#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int main(void) {
    struct pstat ps;

    if (getpinfo(&ps) < 0) {
        printf(1, "getpinfo failed\n");
        exit();
    }

    printf(1, "PID\tTICKS\tTICKETS\tNAME\n");
    printf(1, "------------------------------------\n");

    for (int i = 0; i < NPROC; i++) {
        if (ps.inuse[i] == 1) {
            printf(1, "%d\t%d\t%d\t%s\n", 
                   ps.pid[i], 
                   ps.ticks[i], 
                   ps.tickets[i], 
                   ps.name[i]);
        }
    }

    exit();
}
