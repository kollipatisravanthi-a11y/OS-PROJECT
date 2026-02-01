#include "uthread.h"
#include <string.h>
#include <stdlib.h>

TCB tcb[MAX_THREADS];
int current = -1;
int thread_count = 0;
int running = 1;
int semaphore_count = 0;

int physical_memory[PHYSICAL_PAGES];

#ifdef _WIN32
    CRITICAL_SECTION lock;
    HANDLE timer_thread;
    LPVOID main_fiber;
#else
    pthread_mutex_t lock;
    pthread_t timer_thread;
    ucontext_t main_context;
#endif

FILE *logf;

long long now() {
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

void log_event(const char *t, const char *a) {
    if (!logf) return;
    fprintf(logf, "%lld %s %s\n", now(), t, a);
    fflush(logf);
}

#ifndef _WIN32
void linux_entry() {
    int id = current;
    tcb[id].func(tcb[id].arg);
    uthread_exit();
}
#else
void fiber_entry(void *arg) {
    int id = (int)(size_t)arg;
    tcb[id].func(tcb[id].arg);
    uthread_exit();
}
#endif

#ifdef _WIN32
DWORD WINAPI timer_func(LPVOID arg) {
    while (running) {
        Sleep(10);
        uthread_timer_tick();
    }
    return 0;
}
#else
void* timer_func(void* arg) {
    while (running) {
        usleep(10 * 1000);
        uthread_timer_tick();
    }
    return NULL;
}
#endif

void uthread_init() {
    logf = fopen("scheduler_log.txt", "w");
    log_event("SYSTEM", "INIT");
#ifdef _WIN32
    InitializeCriticalSection(&lock);
    main_fiber = ConvertThreadToFiber(NULL);
#else
    pthread_mutex_init(&lock, NULL);
#endif
    for(int i=0; i<PHYSICAL_PAGES; i++) physical_memory[i] = -1;
    semaphore_count = 0;
}

int uthread_create(void (*func)(void *), void *arg, int priority) {
    if (thread_count >= MAX_THREADS) return -1;

    TCB *t = &tcb[thread_count];
    t->id = thread_count;
    t->state = READY;
    t->priority = (priority >= 0 && priority < MLFQ_LEVELS) ? priority : 0;
    t->age = 0;
    t->quantum_used = 0;
    t->func = func;
    t->arg = arg;
    t->waiting_for = -1;
    for(int i=0; i<5; i++) t->holding_locks[i] = -1;
    
    for(int i=0; i<VIRTUAL_PAGES; i++) t->page_table[i] = -1;
    sprintf(t->name, "T%d", t->id);

#ifdef _WIN32
    t->fiber = CreateFiber(STACK_SIZE, fiber_entry, (void *)(size_t)t->id);
#else
    getcontext(&t->context);
    t->context.uc_stack.ss_sp = t->stack;
    t->context.uc_stack.ss_size = STACK_SIZE;
    t->context.uc_link = &main_context;
    makecontext(&t->context, linux_entry, 0);
#endif

    log_event(t->name, "CREATED");
    return thread_count++;
}

void uthread_start() {
    log_event("SYSTEM", "START");
#ifdef _WIN32
    timer_thread = CreateThread(NULL, 0, timer_func, NULL, 0, NULL);
#else
    pthread_create(&timer_thread, NULL, timer_func, NULL);
#endif

    while (1) {
#ifdef _WIN32
        EnterCriticalSection(&lock);
#else
        pthread_mutex_lock(&lock);
#endif

        int next = -1;
        for (int q = 0; q < MLFQ_LEVELS; q++) {
            for (int i = 0; i < thread_count; i++) {
                if (tcb[i].state == READY && tcb[i].priority == q) {
                    next = i;
                    goto found;
                }
            }
        }
        
        found:;
        char rq_buf[256] = "MLFQ: ";
        for (int q = 0; q < MLFQ_LEVELS; q++) {
            char lvl[10]; sprintf(lvl, "Q%d[", q); strcat(rq_buf, lvl);
            for (int i = 0; i < thread_count; i++) {
                if (tcb[i].state == READY && tcb[i].priority == q) {
                    char item[10]; sprintf(item, "%s ", tcb[i].name);
                    strcat(rq_buf, item);
                }
            }
            strcat(rq_buf, "] ");
        }
        log_event("SYSTEM", rq_buf);

        if (next == -1) {
            int active = 0;
            for(int i=0; i<thread_count; i++) if(tcb[i].state != FINISHED) active = 1;
            
            if (!active) {
                running = 0;
#ifdef _WIN32
                LeaveCriticalSection(&lock);
#else
                pthread_mutex_unlock(&lock);
#endif
                break;
            } else {
#ifdef _WIN32
                LeaveCriticalSection(&lock);
                Sleep(10);
#else
                pthread_mutex_unlock(&lock);
                usleep(10000);
#endif
                continue;
            }
        }

        current = next;
        tcb[current].state = RUNNING;
        tcb[current].quantum_used = 0;
        log_event(tcb[current].name, "RUNNING");

#ifdef _WIN32
        LeaveCriticalSection(&lock);
        SwitchToFiber(tcb[current].fiber);
#else
        pthread_mutex_unlock(&lock);
        swapcontext(&main_context, &tcb[current].context);
#endif
    }
}

void uthread_timer_tick() {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif

    if (current != -1 && tcb[current].state == RUNNING) {
        tcb[current].quantum_used += 10;
        int limit = (tcb[current].priority == 0) ? Q0_QUANTUM : Q1_QUANTUM;
        
        if (tcb[current].priority < MLFQ_LEVELS - 1 && tcb[current].quantum_used >= limit) {
            int old_q = tcb[current].priority;
            tcb[current].priority++;
            tcb[current].state = READY;
            char buf[50]; sprintf(buf, "MLFQ_DOWNGRADE Q%d->Q%d", old_q, tcb[current].priority);
            log_event(tcb[current].name, buf);
            
#ifdef _WIN32
            LeaveCriticalSection(&lock);
            SwitchToFiber(main_fiber);
#else
            pthread_mutex_unlock(&lock);
            swapcontext(&tcb[current].context, &main_context);
#endif
            return;
        }
    }
    
    // Simulate I/O Completion (Random)
    for(int i=0; i<thread_count; i++) {
        if(tcb[i].state == DISK_WAIT && (rand() % 10) == 0) {
            tcb[i].state = READY;
            log_event(tcb[i].name, "DISK_IO_DONE");
        }
    }

    static int boost_counter = 0;
    boost_counter += 10;
    if (boost_counter >= BOOST_INTERVAL) {
        boost_counter = 0;
        for(int i=0; i<thread_count; i++) if (tcb[i].state != FINISHED) tcb[i].priority = 0;
        log_event("SYSTEM", "MLFQ_BOOST_ALL_TO_Q0");
    }

#ifdef _WIN32
    LeaveCriticalSection(&lock);
#else
    pthread_mutex_unlock(&lock);
#endif
}

void uthread_exit() {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif
    tcb[current].state = FINISHED;
    log_event(tcb[current].name, "FINISHED");
    for(int i=0; i<5; i++) tcb[current].holding_locks[i] = -1;
    for(int i=0; i<PHYSICAL_PAGES; i++) if(physical_memory[i] == current) physical_memory[i] = -1;

#ifdef _WIN32
    LeaveCriticalSection(&lock);
    SwitchToFiber(main_fiber);
#else
    pthread_mutex_unlock(&lock);
    setcontext(&main_context);
#endif
}

void uthread_yield() {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif
    tcb[current].state = READY;
    log_event(tcb[current].name, "YIELD");
#ifdef _WIN32
    LeaveCriticalSection(&lock);
    SwitchToFiber(main_fiber);
#else
    pthread_mutex_unlock(&lock);
    swapcontext(&tcb[current].context, &main_context);
#endif
}

/* ---------- DISK I/O ---------- */
void uthread_disk_io(int block_id) {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif
    tcb[current].state = DISK_WAIT;
    char buf[50]; sprintf(buf, "DISK_IO_START %d", block_id);
    log_event(tcb[current].name, buf);
#ifdef _WIN32
    LeaveCriticalSection(&lock);
    SwitchToFiber(main_fiber);
#else
    pthread_mutex_unlock(&lock);
    swapcontext(&tcb[current].context, &main_context);
#endif
}

/* ---------- SEMAPHORES ---------- */
void uthread_sem_init(uthread_sem_t *sem, int initial_value) {
    static int id_gen = 100;
    sem->id = id_gen++;
    sem->value = initial_value;
    sem->queue_size = 0;
    sem->owner_id = -1;
}

void uthread_sem_wait(uthread_sem_t *sem) {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif

    if (sem->value <= 0) {
        sem->blocked_queue[sem->queue_size++] = current;
        tcb[current].state = BLOCKED;
        tcb[current].waiting_for = sem->id;
        
        char buf[50]; sprintf(buf, "BLOCKED_ON_SEM %d_OWNED_BY_%d", sem->id, sem->owner_id);
        log_event(tcb[current].name, buf);
        
#ifdef _WIN32
        LeaveCriticalSection(&lock);
        SwitchToFiber(main_fiber);
#else
        pthread_mutex_unlock(&lock);
        swapcontext(&tcb[current].context, &main_context);
#endif
    } else {
        sem->value--;
        sem->owner_id = current;
        tcb[current].waiting_for = -1;
        // Track ownership
        for(int i=0; i<5; i++) if(tcb[current].holding_locks[i] == -1) { tcb[current].holding_locks[i] = sem->id; break; }
        
        char buf[50]; sprintf(buf, "ACQUIRED_SEM %d", sem->id);
        log_event(tcb[current].name, buf);
#ifdef _WIN32
        LeaveCriticalSection(&lock);
#else
        pthread_mutex_unlock(&lock);
#endif
    }
}

void uthread_sem_post(uthread_sem_t *sem) {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif

    // Remove ownership
    for(int i=0; i<5; i++) if(tcb[current].holding_locks[i] == sem->id) tcb[current].holding_locks[i] = -1;
    sem->owner_id = -1;

    if (sem->queue_size > 0) {
        int next_thread = sem->blocked_queue[0];
        for(int i=0; i<sem->queue_size-1; i++) sem->blocked_queue[i] = sem->blocked_queue[i+1];
        sem->queue_size--;
        
        tcb[next_thread].state = READY;
        tcb[next_thread].waiting_for = -1;
        
        // Ownership transfers to the unblocked thread
        sem->owner_id = next_thread;
        for(int i=0; i<5; i++) if(tcb[next_thread].holding_locks[i] == -1) { tcb[next_thread].holding_locks[i] = sem->id; break; }

        char buf[50]; sprintf(buf, "SIGNAL_HANDOVER %d_TO_%s", sem->id, tcb[next_thread].name);
        log_event("SYSTEM", buf);
        log_event(tcb[next_thread].name, "UNBLOCKED_BY_SEM");
    } else {
        sem->value++;
    }

#ifdef _WIN32
    LeaveCriticalSection(&lock);
#else
    pthread_mutex_unlock(&lock);
#endif
}

int uthread_mutex_init(uthread_mutex_t *m) { uthread_sem_init(m, 1); return 1; }
int uthread_mutex_lock(uthread_mutex_t *m) { uthread_sem_wait(m); return 1; }
int uthread_mutex_unlock(uthread_mutex_t *m) { uthread_sem_post(m); return 1; }

void* uthread_malloc(size_t size) {
    int pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    int first_page = -1;
    for(int i=0; i < VIRTUAL_PAGES && pages_needed > 0; i++) {
        if (tcb[current].page_table[i] == -1) {
            int p_page = -1;
            for(int j=0; j<PHYSICAL_PAGES; j++) if (physical_memory[j] == -1) { p_page = j; physical_memory[j] = current; break; }
            if (p_page == -1) {
                p_page = rand() % PHYSICAL_PAGES;
                log_event("SYSTEM", "PAGE_REPLACEMENT_LRU");
                physical_memory[p_page] = current;
            }
            tcb[current].page_table[i] = p_page;
            char buf[50]; sprintf(buf, "PAGE_FAULT_MAPPED V:%d->P:%d", i, p_page);
            log_event(tcb[current].name, buf);
            if (first_page == -1) first_page = i;
            pages_needed--;
        }
    }
    return (void*)(size_t)(first_page * PAGE_SIZE);
}
void uthread_free(void* ptr) {
    // Simulated free: clears all mappings for this thread
    for(int i=0; i<VIRTUAL_PAGES; i++) {
        int p_page = tcb[current].page_table[i];
        if (p_page != -1) {
            physical_memory[p_page] = -1;
            tcb[current].page_table[i] = -1;
        }
    }
    log_event(tcb[current].name, "MEMORY_FREE_ALL"); 
}
