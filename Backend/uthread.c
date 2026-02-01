/* 
 * Advanced Multi-Core Thread Visualizer 
 * Cross-Platform User-Level Scheduling Core
 */
#include "uthread.h"
#include <string.h>
#include <stdlib.h>

TCB tcb[MAX_THREADS];
int current = -1;
int thread_count = 0;
int running = 1;

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

/* ---------- LOGGING ---------- */
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

/* ---------- ENTRY ---------- */
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

/* ---------- TIMER ---------- */
#ifdef _WIN32
DWORD WINAPI timer_func(LPVOID arg) {
    while (running && PREEMPTIVE) {
        Sleep(TIME_SLICE);
        uthread_timer_tick();
    }
    return 0;
}
#else
void* timer_func(void* arg) {
    while (running && PREEMPTIVE) {
        usleep(TIME_SLICE * 1000);
        uthread_timer_tick();
    }
    return NULL;
}
#endif

/* ---------- INIT ---------- */
void uthread_init() {
    logf = fopen("scheduler_log.txt", "w");
    log_event("SYSTEM", "INIT");
#ifdef _WIN32
    InitializeCriticalSection(&lock);
    main_fiber = ConvertThreadToFiber(NULL);
#else
    pthread_mutex_init(&lock, NULL);
#endif
}

/* ---------- CREATE ---------- */
int uthread_create(void (*func)(void *), void *arg, int priority) {
    if (thread_count >= MAX_THREADS) return -1;

    TCB *t = &tcb[thread_count];
    t->id = thread_count;
    t->state = READY;
    t->priority = priority;
    t->age = 0;
    t->func = func;
    t->arg = arg;

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

/* ---------- SCHEDULER ---------- */
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

        int next = -1, best = -9999;

        for (int i = 0; i < thread_count; i++) {
            if (tcb[i].state == READY) {
                int old_p = tcb[i].priority + tcb[i].age;
                tcb[i].age++;                       // aging
                int new_p = tcb[i].priority + tcb[i].age;
                
                char aging_buf[50];
                sprintf(aging_buf, "AGING P:%d->%d", old_p, new_p);
                log_event(tcb[i].name, aging_buf);

                int score = new_p;
                if (score > best) {
                    best = score;
                    next = i;
                }
            }
        }

        // Log Ready Queue
        char rq_buf[256] = "RQ: ";
        for (int i = 0; i < thread_count; i++) {
            if (tcb[i].state == READY) {
                char item[32];
                sprintf(item, "[%s|P=%d] ", tcb[i].name, tcb[i].priority + tcb[i].age);
                strcat(rq_buf, item);
            }
        }
        log_event("SYSTEM", rq_buf);

        if (next == -1) {
            running = 0;
#ifdef _WIN32
            LeaveCriticalSection(&lock);
#else
            pthread_mutex_unlock(&lock);
#endif
            break;
        }

        current = next;
        tcb[current].state = RUNNING;
        tcb[current].age = 0;

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

/* ---------- TIMER PREEMPT ---------- */
void uthread_timer_tick() {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif

    if (current != -1 && tcb[current].state == RUNNING) {
        tcb[current].state = READY;
        char preempt_buf[50];
        sprintf(preempt_buf, "PREEMPTED_TICK");
        log_event(tcb[current].name, preempt_buf);
#ifdef _WIN32
        LeaveCriticalSection(&lock);
        SwitchToFiber(main_fiber);
#else
        pthread_mutex_unlock(&lock);
        swapcontext(&tcb[current].context, &main_context);
#endif
        return;
    }

#ifdef _WIN32
    LeaveCriticalSection(&lock);
#else
    pthread_mutex_unlock(&lock);
#endif
}

/* ---------- EXIT ---------- */
void uthread_exit() {
#ifdef _WIN32
    EnterCriticalSection(&lock);
#else
    pthread_mutex_lock(&lock);
#endif
    tcb[current].state = FINISHED;
    log_event(tcb[current].name, "FINISHED");
#ifdef _WIN32
    LeaveCriticalSection(&lock);
    SwitchToFiber(main_fiber);
#else
    pthread_mutex_unlock(&lock);
    setcontext(&main_context);
#endif
}

/* ---------- YIELD ---------- */
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

/* ---------- MUTEX ---------- */
int uthread_mutex_init(uthread_mutex_t *m) {
#ifdef _WIN32
    *m = CreateMutex(NULL, FALSE, NULL);
    return (*m != NULL);
#else
    return (pthread_mutex_init(m, NULL) == 0);
#endif
}

int uthread_mutex_lock(uthread_mutex_t *m) {
#ifdef _WIN32
    WaitForSingleObject(*m, INFINITE);
#else
    pthread_mutex_lock(m);
#endif
    return 1;
}

int uthread_mutex_unlock(uthread_mutex_t *m) {
#ifdef _WIN32
    ReleaseMutex(*m);
#else
    pthread_mutex_unlock(m);
#endif
    return 1;
}
