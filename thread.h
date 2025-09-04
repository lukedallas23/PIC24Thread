#ifndef THREAD_H
#define	THREAD_H

#ifdef	__cplusplus
extern "C" {
#endif
  
#define MAX_STACK 0x7fff    // Use if wanting to allocate max stack size
#define DEFUALT_INTTERRUPT 0
#define THREAD_LOCK_INITILIZE 1
#define COND_VAR_INITILIZE 0
#define NULL 0

typedef struct s_TCB {
    unsigned int PC;    // Program counter (updated when there is a context switch, so that thread returns back to correct position)
    unsigned int SP;    // Stack pointer (updated when there is a context switch, so that the stack return back to correct position)
    int join_id;        // Thread join ID (if joining)
} TCB;

typedef unsigned int thread_lock;
typedef unsigned int thread_cv;
    
void disable_interrupts();
void enable_interrupts();
void thread_lock_aquire(thread_lock* lock);
void thread_lock_release(thread_lock* lock);
void thread_lock_wait(thread_lock* lock, thread_cv* cv);
void thread_lock_signal(thread_cv* cv);
void thread_lock_broadcast(thread_cv* cv);
int switch_threads();
int thread_init(unsigned int s_size, unsigned int max_threads, unsigned int timer_ms);
void thread_yield();
int thread_join(int join_id);
void thread_exit();
int thread_create(void* (*routine)(void*), void *arg);



#ifdef	__cplusplus
}
#endif

#endif	/* THREAD_H */

