#ifndef THREADS_H  

#define THREADS_H

typedef void (*start_routine)(void* arg);
typedef struct {
	volatile int turn;
	int ticket;
} lock_t;

int thread_create(start_routine fcn, void* arg);
int thread_join();

void lock_init(lock_t* lock);
void lock_acquire(lock_t* lock);
void lock_release(lock_t* lock);

#endif // THREADS_H
