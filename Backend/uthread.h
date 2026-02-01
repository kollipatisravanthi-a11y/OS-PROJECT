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

/* MLFQ Constants */
#define MLFQ_LEVELS 3
#define Q0_QUANTUM 50   // ms
#define Q1_QUANTUM 100  // ms
#define BOOST_INTERVAL 1000 // ms

/* Memory Constants */
#define PAGE_SIZE 4096
#define VIRTUAL_PAGES 16
#define PHYSICAL_PAGES 8

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    DISK_WAIT,
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
    int priority;       // MLFQ Level (0=High, 1=Med, 2=Low)
    int age;            // For priority boosting
    int quantum_used;   // Track time spent in current quantum
    void (*func)(void *);
    void *arg;
    char name[10];
    
    // Memory Simulation
    int page_table[VIRTUAL_PAGES]; // Virtual Page -> Physical Page (-1 if not mapped)
    
    // Resource Tracking
    int holding_locks[5]; // IDs of semaphores held by this thread
    int waiting_for;      // ID of semaphore this thread is blocked on (-1 if none)
} TCB;

/* Semaphore API */
typedef struct {
    int id;
    int value;
    int blocked_queue[MAX_THREADS];
    int queue_size;
    int owner_id; // For mutex-like behavior tracking
} uthread_sem_t;

/* Mutex API (Re-implemented using Semaphores) */
typedef uthread_sem_t uthread_mutex_t;

/* Thread API */
void uthread_init();
int  uthread_create(void (*func)(void *), void *arg, int priority);
void uthread_start();
void uthread_exit();
void uthread_yield();
void uthread_timer_tick();

/* Sync API */
void uthread_sem_init(uthread_sem_t *sem, int initial_value);
void uthread_sem_wait(uthread_sem_t *sem);
void uthread_sem_post(uthread_sem_t *sem);

int uthread_mutex_init(uthread_mutex_t *m);
int uthread_mutex_lock(uthread_mutex_t *m);
int uthread_mutex_unlock(uthread_mutex_t *m);

/* Disk I/O Simulation */
void uthread_disk_io(int block_id);

/* Memory Simulation API */
void* uthread_malloc(size_t size);
void  uthread_free(void* ptr);
int   uthread_mmu_translate(int virtual_addr);

#endif
