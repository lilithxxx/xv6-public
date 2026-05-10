#include "types.h"
#include "user.h"
#include "threads.h"

#define N_THREADS 5
#define N_ITERS 1000

// Shared globals
int shared_counter = 0;
lock_t my_lock;

// The function each thread will run
void worker(void *arg_ptr) {
    int id = *(int*)arg_ptr;
    
    printf(1, "Thread %d starting up...\n", id);

    for (int i = 0; i < N_ITERS; i++) {
        // 1. Stress test the ticket lock
        lock_acquire(&my_lock);
        shared_counter++;
        lock_release(&my_lock);
        
        // 2. Stress test the global sbrk lock (concurrent heap allocations)
        // This will crash spectacularly if your sysproc.c lock is broken!
        void *temp_mem = malloc(16);
        if (temp_mem != 0) {
            free(temp_mem);
        }
    }

    printf(1, "Thread %d finished its work.\n", id);
    
    // CRITICAL: Because we pushed a fake return address (0xffffffff) to the stack
    // in sys_clone, threads cannot "return". They must explicitly call exit().
    exit(); 
}

int main(void) {
    printf(1, "--- Starting xv6 Thread Stress Test ---\n");

    lock_init(&my_lock);

    int thread_ids[N_THREADS];
    int pids[N_THREADS];

    // Spawn the threads
    for (int i = 0; i < N_THREADS; i++) {
        thread_ids[i] = i + 1; // Give each thread a unique ID (1 to 5)
        pids[i] = thread_create(worker, &thread_ids[i]);
        
        if (pids[i] < 0) {
            printf(1, "PANIC: Failed to create thread %d\n", i + 1);
            exit();
        }
        printf(1, "Main: Created thread %d with PID %d\n", thread_ids[i], pids[i]);
    }

    // Wait for all threads to finish and clean up their stacks
    for (int i = 0; i < N_THREADS; i++) {
        int joined_pid = thread_join();
        if (joined_pid < 0) {
            printf(1, "PANIC: thread_join failed!\n");
        } else {
            printf(1, "Main: Successfully joined thread PID %d\n", joined_pid);
        }
    }

    // Verify the results
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
