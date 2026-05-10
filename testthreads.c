#include "types.h"
#include "user.h"
#include "threads.h"

#define N_THREADS 5
#define N_ITERS 1000

int shared_counter = 0;
lock_t my_lock;

void worker(void *arg_ptr) {
    int id = *(int*)arg_ptr;
    
    printf(1, "Thread %d starting up...\n", id);

    for (int i = 0; i < N_ITERS; i++) {
        lock_acquire(&my_lock);
        shared_counter++;
        lock_release(&my_lock);
        
        void *temp_mem = malloc(16);
        if (temp_mem != 0) {
            free(temp_mem);
        }
    }

    printf(1, "Thread %d finished its work.\n", id);
    exit(); 
}

int main(void) {
    lock_init(&my_lock);

    int thread_ids[N_THREADS];
    int pids[N_THREADS];

    for (int i = 0; i < N_THREADS; i++) {
        thread_ids[i] = i + 1; // Give each thread a unique ID (1 to 5)
        pids[i] = thread_create(worker, &thread_ids[i]);
        
        if (pids[i] < 0) {
            printf(1, "PANIC: Failed to create thread %d\n", i + 1);
            exit();
        }
        printf(1, "Main: Created thread %d with PID %d\n", thread_ids[i], pids[i]);
    }

    for (int i = 0; i < N_THREADS; i++) {
        int joined_pid = thread_join();
        if (joined_pid < 0) {
            printf(1, "PANIC: thread_join failed!\n");
        } else {
            printf(1, "Main: Successfully joined thread PID %d\n", joined_pid);
        }
    }

    int expected_total = N_THREADS * N_ITERS;
    printf(1, "\n--- Test Results ---\n");
    printf(1, "Expected Counter: %d\n", expected_total);
    printf(1, "Actual Counter:   %d\n", shared_counter);

    if (shared_counter == expected_total) {
        printf(1, "STATUS: PASSED! You are officially a kernel hacker.\n");
    } else {
        printf(1, "STATUS: FAILED! Race condition detected in lock.\n");
    }

    exit();
}
