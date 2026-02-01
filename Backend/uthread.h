#ifndef UTHREAD_H
#define UTHREAD_H

#ifdef _WIN32
    #include <windows.h>
#else
    #define _XOPEN_SOURCE 600
    #include <ucontext.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/time.h>
#endif

#include <stdio.h>

#define MAX_THREADS 10
#define STACK_SIZE 32768

#define PREEMPTIVE 1        // 1 = preemptive, 0 = cooperative
#define TIME_SLICE 50       // ms (Round Robin)

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} thread_state;

typedef struct {
    int id;
#ifdef _WIN32
    LPVOID fiber;
#else
    ucontext_t context;
    char stack[STACK_SIZE];
#endif
    thread_state state;
    int priority;
    int age;
    void (*func)(void *);
    void *arg;
    char name[10];
} TCB;

#ifdef _WIN32
    typedef HANDLE uthread_mutex_t;
#else
    typedef pthread_mutex_t uthread_mutex_t;
#endif

/* Thread API */
void uthread_init();
int  uthread_create(void (*func)(void *), void *arg, int priority);
void uthread_start();
void uthread_exit();
void uthread_yield();
void uthread_timer_tick();

/* Mutex API */
int uthread_mutex_init(uthread_mutex_t *m);
int uthread_mutex_lock(uthread_mutex_t *m);
int uthread_mutex_unlock(uthread_mutex_t *m);

#endif
