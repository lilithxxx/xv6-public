#include "types.h"
#include "stat.h"
#include "user.h"
#include "threads.h"

#define N_THREADS 5

void spinner(void *arg) {
    int id = *(int*)arg;
    printf(1, "Thread %d spinning...\n", id);
    
    while(1)
		;
}

int main(void) {
    printf(1, "Starting %d spinning threads...\n", N_THREADS);
    
    int ids[N_THREADS];

    for (int i = 0; i < N_THREADS; i++) {
        ids[i] = i + 1;
        if (thread_create(spinner, &ids[i]) < 0) {
            printf(1, "Failed to create thread %d\n", ids[i]);
        }
    }

    for (int i = 0; i < N_THREADS; i++) {
        thread_join();
    }

    exit();
}
