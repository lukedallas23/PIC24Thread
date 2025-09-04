Library which provides thread and synchronization for PIC24 microcontrollers.

To use, include the library and call `thread_init(unsigned int s_size, unsigned int max_threads, unsigned int timer_ms)`
with size of each stack, maximum number of threads (16 is highest), and time before automatic context switching.  

Thread Functions: 
`thread_create`: Create a thread  
`thread_exit`: Called when a thread is to be removed (at the end of execution)  
`thread_join`: Block a thread until another one finishes  
`thread_yield`: Switch context and reset interrupt timer  

Synchronization Functions:
`thread_lock_aquire`: Aquire a lock and block until avaliable  
`thread_lock_release`: Release a lock  
`thread_lock_wait`: Yeild control and wait on a condition variable  
`thread_lock_signal`: Signal a waiting thread  
`thread_lock_broadcast`: Signal all waiting threads  
