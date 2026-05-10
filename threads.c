#include <threads.h>
#include <types.h>
#include <user.h>
#include <mmu.h>
#include <x86.h>

int thread_create(start_routine fcn, void *arg) {
	void* stack = malloc(PGSIZE * 2);
	if(stack == 0)
		return -1;
	return clone(fcn, arg, stack);
}

int thread_join() {
	void* stack;
	int pid = join(&stack);
	if(pid == -1)
		return -1;
	free(stack);
	return pid;
}

void lock_init(lock_t* lock) {
	lock->turn = 0;
	lock->ticket = 0;
}

void lock_acquire(lock_t* lock) {
	int ticket = fetchAndAdd(&lock->ticket, 1);
	while(ticket != lock->turn)
		; // spin lock
}

void lock_release(lock_t* lock) {
	lock->turn++;
}
