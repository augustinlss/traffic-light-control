/* 
 * Operating Systems <2INC0> Practical 
 * 2016/2020 (c) Joris Geurts
 *
 * This program contains some C constructs which might be useful for
 * the Threaded Application assignment of 2INC0.
 *
 *
 *      I M P O R T A N T    M E S S A G E :
 *      ====================================
 *
 * For readability reasons, this program does not check the return value of 
 * the POSIX calls.
 * This is not a good habit.
 * Always check the return value of a system call (you never know if the disk is
 * is full, or if we run out of other system resources)!
 * Possible construction:
 *
 *      rtnval = <posix-call>();
 *      if (rtnval == <error-value-according-documentation>)
 *      {
 *          perror ("<your-message>");
 *          exit (1);
 *      }
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>


// declare a mutex, and it is initialized as well
static pthread_mutex_t      mutex          = PTHREAD_MUTEX_INITIALIZER;

// declare a semaphore
static sem_t semaphore;

// declare a global integer
static int globalint;

/*-------------------------------------------------------------------------*/

    
static void *
my_thread (void * arg)
{
    int *   argi; 
    int     i;      
    int *   rtnval;
    
    argi = (int *) arg;     // proper casting before dereferencing (could also be done in one statement)
    i = *argi;              // get the integer value of the pointer
    free (arg);             // we retrieved the integer value, so now the pointer can be deleted
    
    printf ("        %lx: thread started; parameter=%d\n", pthread_self(), i);
    
    sleep (1);

    // make the thread change the global integer
    globalint = 33;
    
    // a return value to be given back to the calling main-thread
    rtnval = malloc (sizeof (int)); // will be freed by the parent-thread
    *rtnval = 42;           // assign an arbitrary value...
    return (rtnval);        // you can also use pthread_exit(rtnval);
}


static void
thread_test (void)
{
    // set he global integer to 0
    globalint = 0;

    int *       parameter;
    int *       rtnval;
    pthread_t   thread_id;
    
    // parameter to be handed over to the thread
    parameter = malloc (sizeof (int));  // memory will be freed by the child-thread
    *parameter = 73;        // assign an arbitrary value...
    
    printf ("%lx: starting thread ...\n", pthread_self());
    pthread_create (&thread_id, NULL, my_thread, parameter);
    
    sleep (3);
    
    // wait for the thread, and we are interested in the return value
    pthread_join (thread_id, (void **) &rtnval);
    
    printf ("%lx: thread ready; return value=%d\n", pthread_self(), *rtnval);
    free (rtnval);          // free the memory that has been allocated by the thread
    printf ("\n");

    printf ("globalint=%d\n", globalint); // did the global integer change?
}


static void *
my_mutex_thread (void * arg)
{
    printf ("        %lx: thread start; wanting to enter CS...\n", pthread_self());
    pthread_mutex_lock (&mutex);
    
    // Here we are in the critical section (protected by a mutex)
    printf ("        %lx: thread entered CS\n", pthread_self());
    sleep (10);
    printf ("        %lx: thread leaves CS\n", pthread_self());
    
    pthread_mutex_unlock (&mutex);
    
    return (NULL);
}


static void
thread_mutex_test (void)
{
    pthread_t   my_threads[2];

    printf ("%lx: starting first thread ...\n", pthread_self());
    pthread_create (&my_threads[0], NULL, my_mutex_thread, NULL);
    sleep (3);
    printf ("%lx: starting second thread ...\n", pthread_self());
    pthread_create (&my_threads[1], NULL, my_mutex_thread, NULL);
    
    // wait for threads, but we are not interested in the return value 
    pthread_join (my_threads[0], NULL);
    pthread_join (my_threads[1], NULL); 
    
    printf ("%lx: threads ready\n", pthread_self());
    printf ("\n");
}


static void *
my_semaphore_thread (void * arg)
{
    printf ("        %lx: thread start; wanting for signal...\n", pthread_self());
    sem_wait(&semaphore);
    printf ("        %lx: signal received!\n", pthread_self());
        
    return (NULL);
}


static void
thread_semaphore_test (void)
{
    pthread_t my_thread;
    

    // we create a semaphore and initialise it at 0
    // pshared: the second parameter, 
    //      -> 0: The semaphore is shared only between threads of the same process.
    //      -> 1: The semaphore is shared between processes, 
    //          and it should be placed in a shared memory segment accessible to multiple processes.
    sem_init(&semaphore, 0, 0);

    printf ("%lx: starting thread ...\n", pthread_self());
    pthread_create (&my_thread, NULL, my_semaphore_thread, NULL);

    // wait a bit
    sleep (3);

    // send a signal by incrementing the semaphore
    printf ("%lx: incrementing semaphore\n", pthread_self());
    sem_post(&semaphore);

    // wait for threads, but we are not interested in the return value 
    pthread_join (my_thread, NULL);
    
    printf ("%lx: threads ready\n", pthread_self());
    printf ("\n");
}


/*-------------------------------------------------------------------------*/

int main (void)
{
    thread_test();
    thread_mutex_test();
    thread_semaphore_test();
    
    return (0);
}

