#include "xc.h"
#include "template.h"
#include "thread.h"

// Start of stack for each thread is ThreadID*stack_size + stack_st
volatile unsigned int stack_st       = 0x800;    // Address of the start of stack on main
volatile unsigned int stack_size     = 0;        // Stack size per thread
volatile unsigned int max_threads    = 16;       // Max threads

volatile TCB TCBs[16];       // Max of 16 threads (15 newly defined)

// Thread tracking variables
volatile unsigned int ALLOCATED  = 1;    // Keeps track of which threads are ALLOCATED
volatile unsigned int READY      = 1;    // Keeps track of which threads are READY (and running)
volatile unsigned int FINISHED   = 0;    // Keeps track of which threads are FINISHED (but not joined)
volatile unsigned int running    = 0;    // ID of thread running

volatile unsigned int exitInterruptPriority = 0; // Resotred priority of interrupts when reenabled

// Disables interrupts until enable_interrupts is called by moving the CPU Interrupt Priority
// Bits to 7. (Prioirty 7 interrupts (traps) will still run)
void disable_interrupts() {
    SRbits.IPL = 7;                     // Disables interrupts
}

// Reenables interrupts by moving the CPU interrupt priority bits to previously saved level
void enable_interrupts() {
    SRbits.IPL = exitInterruptPriority;
}

// Checks if lock is avabile. If yes, this returns. If no, this thread will yeild
// CPU control and continue to check until the lock is free, at which point this will return.
void thread_lock_aquire(thread_lock* lock) {
    disable_interrupts();
    while(!(*lock)) {
        switch_threads();
        disable_interrupts();
    }
    *lock = 0;      // The lock is aquired by this thread/       
    enable_interrupts();
}

// Frees the lock and then returns.
void thread_lock_release(thread_lock* lock) {
    disable_interrupts();
    *lock = 1;
    enable_interrupts();
}

/*
    This should be called when the thread has aquierd the corresponding lock.
    This will take the thread off of RUNNING list, release the lock, and then
    yield control. This should be contained in a while loop where it is called,
    as a signal does not guarentee that condition is met as it does not run immediately,
    , and may need to be waited again.
*/
void thread_lock_wait(thread_lock* lock, thread_cv* cv) {
    disable_interrupts();
    *cv |= (1 << running);     // Put this thread in waiting variable
    READY  &= (0xffff - (1 << running));    // The thread is moved from ready and isn't run until signaled
    thread_lock_release(lock);
    switch_threads();
    thread_lock_aquire(lock);
}

/*
    Takes the lowest ID thread which is waiting and moves it back to READY.
    This does not release the lock and execution of the signaled thread does
    not begin immediatly after this is called.
*/
void thread_lock_signal(thread_cv* cv) {
    disable_interrupts();
    for (int i = 0; i < max_threads; i++) {
        if (*cv & (1 << i)) {
            READY |= (1 << i);              // Move signaled thread to READY
            break;
        }
    }
    enable_interrupts();
}

/*
    Takes all threads which are waiting and moves them back to READY.
    This does not release the lock and execution of the signaled threads do
    not begin immediatly after this is called.
*/
void thread_lock_broadcast(thread_cv* cv) {
    disable_interrupts();
    for (int i = 0; i < max_threads; i++) {
        if (*cv & (1 << i)) {
            READY |= (1 << i);              // Move signaled thread to READY
        }
    }
    enable_interrupts();
}

// Switches context to another thread
int switch_threads() {

    disable_interrupts();
    int next_thread = running + 1;      // Thread getting switched to
    for (int i = next_thread; i < (next_thread + max_threads); i++) {
        if ((1 << (i % max_threads)) & READY) {
            next_thread = i % max_threads;
            break;
        }
    }
    volatile int flag = 0;
    // THESE LINES SAVE THE CONTEXT FROM THE LAST THREAD
    push_to_stack();            // Push registers to the stack
    TCBs[running].SP = WREG15;  // Place SP in the TCB
    TCBs[running].PC = PCL+4;     // Place the program counter in the TCB
    
    // Switch context, the new threads context will start after PC is set
    if (flag == 0) {
        flag = 1;
        running = next_thread;
        WREG15 = TCBs[next_thread].SP;
        WREG13 = TCBs[next_thread].PC;      // Move PC into arbitary register (old registers already saved)
        bra_to_address();                   // This will move the PC and the next thread offically starts
        // PC CHANGE INSTRUCTION

    }

    if (flag == 1) {

        pop_from_stack();
        TMR1 = 0;
        _T1IF = 0;
        enable_interrupts();
        return 0;
    }
    return -1;  // Should never get here

}


/*
    Initilize the thread library. Specifies thread size, max amount of threads, and
    context switching timer.
*/
int thread_init(unsigned int s_size, unsigned int max_threads, unsigned int timer_ms){

    if (max_threads > 16) max_threads = 16;         // Only supports up to 16 threads

    stack_st = WREG14;                              // Get the starting stack position of main thread
    unsigned int stack_space = SPLIM - stack_st;    // Total stack space avaliable

    stack_size = stack_space/max_threads;           // MAX Total stack size per thread
    if (s_size < stack_size) stack_size = s_size;   // If caller specifies smaller stack size than MAX, use that
    stack_size &= 0xfffe;                           // Stack must be word-alligned
    
    for (int i = 0; i < 16; i++) {
        TCBs[i].join_id = -1;
    }
    
    T1CON = 0;
    TMR1 = 0;
    if (timer_ms <= 4) PR1 = timer_ms*16000;
    else if (timer_ms <= 32) {
        T1CONbits.TCKPS = 1;
        PR1 = timer_ms*2000;
    }
    else if (timer_ms <= 256) {
        T1CONbits.TCKPS = 2;
        PR1 = timer_ms*250;
    }
    else if (timer_ms <= 1047) {
        T1CONbits.TCKPS = 3;
        PR1 = (unsigned int)(timer_ms*62.5);
    } 
    else {
        T1CONbits.TCKPS = 3;
        PR1 = 0xffff;
    }
    
    _T1IF = 0;
    _T1IP = 6;
    _T1IE = 1;
    T1CONbits.TON = 1;
    
    return 0;
}

// Will switch current threads context to that of the next
void thread_yield() {
    _T1IF = 1;
}

/*
    Block until specified thread is FINISHED. If already finished, then 
    return with no blocking

    If join_id is FINISHED - This will not block. That thread is deallocated and taken off FINISHED
    If join_id is not done - This will block. When the corresponding thread exits, it will unblock this
    thread

    Returns 0 on succuss (thread exists) and -1 of failure (thread does not exist)
*/
int thread_join(int join_id){
    disable_interrupts();
    
    // Thread must be properly bounded
    if (join_id >= max_threads) return -1;
    if (join_id < 0) return -1;
    if (join_id == running) return -1;              // Cannot join with yourself
    if (!(ALLOCATED & (1 << join_id))) return -1;   // Thread must be active
    
    // A thread can only be joined once
    for (int i = 0; i < max_threads; i++) {
        if (TCBs[i].join_id == join_id) return -1;
    }
    
    // If thread is FINISHED, eliminate it and return with no blocking
    if (FINISHED & (1 << join_id)) {
        FINISHED  &= (0xffff - (1 << join_id));  // Thread taken out of finished
        ALLOCATED &= (0xffff - (1 << join_id));  // Thread's stack can be given to a new thread
        return 0;
    }
    
    READY &= (0xffff - (1 << running));          // Running thread needs to block
    TCBs[running].join_id = join_id;
    
    switch_threads();                              // Yield control
    
    disable_interrupts();
    READY |= (1 << running);                     // Now that thread is done, thie thread does not block
    enable_interrupts();
    return 0;
    
}

/*
    Called when a thread finishes executing. Will check if a thread has called join on it
        a. If yes, that thread becomes READY, this thread gets deallocated
        b. If no, this thread is added to FINISHED until a thread calls join (join will clean-up)
    Thread is automatically moved off of READY
*/
void thread_exit() {
    
    disable_interrupts();
    READY &= (0xffff - (1 << running));     // This thread is done
    
    for (int i = 0; i < max_threads; i++) {
        if (TCBs[i].join_id == running) {
            READY |= (1 << i);                      // Move joined thread to ready
            ALLOCATED &= (0xffff - (1 << running)); // This thread can be deallocated, already joined
            break;
        }
        if (i == max_threads - 1) FINISHED |= (1 << running);   // Place on FINISH if no joining thread
    }
    
    thread_yield();       // Will never come back

}

void stub(void* (*routine)(void*), void *arg) {
    asm("NOP"); asm("NOP");
    pop_from_stack();
    TMR1 = 0;
    _T1IF = 0;
    enable_interrupts();
    (*routine)(arg);    // Call the routine
    thread_exit();      // In case user does not call this when function done
}

// Creates a new thread, returns its ID number 
int thread_create(void* (*routine)(void*), void *arg) {

    disable_interrupts();
    int thread_id = -1;

    // Find next avaliable thread
    int i = 0;
    for (i = 0; i < max_threads; i++) {
        if (!((1 << i) & ALLOCATED)) {
            thread_id = i;
            break;
        }
    }
    
    if (i == max_threads) return -1;
    
    ALLOCATED |= (1 << i);  // Allocate new thread in ALLOCATED
    READY |= (1 << i);      // Make new thread READY
    
    // Add ARGS and stub address to the stack
    TCBs[i].SP = stack_st + i*stack_size;   // Start of stack
    *(int*) TCBs[i].SP = routine;           // Place routine address on stack
    *(int*) (TCBs[i].SP+2) = arg;           // Place argument address on stack
    TCBs[i].SP += 4;

    int SP = WREG15;            // Save old stack pointer
    WREG15 = TCBs[i].SP;        // Set SP to the new thread (temporarily)   
    push_to_stack();            // Push registers to new stack
    TCBs[i].SP = WREG15;        // Save new SP to the TCB
    WREG15 = SP;                // Move stack pointer back

    *(int*) (TCBs[i].SP-10) = stack_st + i*stack_size;  // Replace FP (W14) with bottom of SP

    TCBs[i].PC = ((int)(stub)) + 2;    // Set the new thread PC to stub 
    TCBs[i].join_id = -1;       // Not joining yet


    enable_interrupts();
    return i;
}

// Interrupt for context switching
void __attribute__((interrupt, auto_psv)) _T1Interrupt() { 
    TMR1 = 0;
    _T1IF = 0;
    switch_threads();
}

