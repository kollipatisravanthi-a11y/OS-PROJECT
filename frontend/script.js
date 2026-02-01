const code = document.getElementById("code");
const lines = document.getElementById("lineNumbers");
const highlight = document.getElementById("highlight");

// ===== Default C Code =====
code.value = `#include <stdio.h>
#include "uthread.h"

void task(void *arg) {
    int id = *(int*)arg;
    for(int i=0;i<5;i++) {
        printf("Thread %d running\\n", id);
        uthread_yield();
    }
    uthread_exit();
}

int main() {
    uthread_init();
    int a=0,b=1,c=2;
    uthread_create(task,&a,2);
    uthread_create(task,&b,1);
    uthread_create(task,&c,3);
    uthread_start();
    return 0;
}`;

// ===== Editor Line Numbers & Highlighting =====
function updateLines() {
    const count = code.value.split("\n").length;
    lines.textContent = "";
    for (let i = 1; i <= count; i++) lines.textContent += i + "\n";
    updateHighlight();
}

function updateHighlight() {
    highlight.innerHTML = Prism.highlight(code.value, Prism.languages.c, 'c');
    highlight.scrollTop = code.scrollTop;
}

code.addEventListener("input", () => {
    code.value = code.value.replace(/^(\d+\s*\n)+/, '');
    updateLines();
});

code.addEventListener("scroll", () => {
    lines.scrollTop = code.scrollTop;
    highlight.scrollTop = code.scrollTop;
});

updateLines();

// ===== Backend Actions =====
// ===== Tabs =====
function showTab(tabName) {
    document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));

    event.currentTarget.classList.add('active');
    document.getElementById(tabName + 'Tab').classList.add('active');
}

// ===== Reset =====
function reset() {
    code.value = templates.basic;
    updateLines();
    document.getElementById("schedulerLog").textContent = "";
    document.getElementById("threadOutput").textContent = "";
    document.getElementById("agingLog").textContent = "";
    document.getElementById("gantt").innerHTML = "";
}

// ===== Backend Actions =====
function run() {
    const runBtn = document.getElementById("runBtn");
    runBtn.disabled = true;
    runBtn.textContent = "⌛ Running...";

    fetch("/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code: code.value })
    })
        .then(res => res.json())
        .then(data => {
            runBtn.disabled = false;
            runBtn.textContent = "▶ Run";

            parseLogs(data.output, data.scheduler_log);
            if (data.gantt) drawGantt(data.gantt);
        })
        .catch(err => {
            runBtn.disabled = false;
            runBtn.textContent = "▶ Run";
            alert("Execution failed: " + err);
        });
}

function parseLogs(threadOut, schedulerIn) {
    const scheduler = document.getElementById("schedulerLog");
    const threads = document.getElementById("threadOutput");
    const aging = document.getElementById("agingLog");

    scheduler.textContent = "";
    threads.textContent = threadOut;
    aging.textContent = "";

    if (!schedulerIn) return;

    const lines = schedulerIn.split('\n');
    lines.forEach(line => {
        if (!line.trim()) return;
        const parts = line.split(' ');
        if (parts.length < 3) return;

        const thread = parts[1];
        const action = parts.slice(2).join(' ');

        const formattedLine = `[${thread}] ${action}`;

        if (action.includes("AGING") || action.includes("RQ:")) {
            aging.textContent += formattedLine + "\n";
        } else if (action.includes("PREEMPTED_TICK")) {
            scheduler.textContent += "[TICK EVENT] " + formattedLine + "\n";
        } else {
            scheduler.textContent += formattedLine + "\n";
        }
    });

    if (threads.textContent.trim().length > 0) {
        // Optional: showTab('threads');
    }
}

function pause() { fetch("/pause", { method: "POST" }); }
function resume() { fetch("/resume", { method: "POST" }); }
function stop() { fetch("/stop", { method: "POST" }); }

// ===== Gantt Chart =====
const actionColors = {
    "RUNNING": "#16a34a",
    "PREEMPTED_TICK": "#dc2626",
    "YIELD": "#2563eb",
    "BLOCKED": "#ea580c",
    "UNBLOCKED": "#9333ea",
    "FINISHED": "#777",
    "CREATED": "#f59e0b",
    "START": "#64748b"
};

function drawGantt(gantt) {
    const box = document.getElementById("gantt");
    box.innerHTML = "";

    if (!gantt || gantt.length === 0) return;

    const totalTime = gantt.reduce((sum, t) => sum + t.time, 0);

    gantt.forEach(t => {
        const d = document.createElement("div");
        d.className = "block" + (t.action === "CREATED" ? " mini-block" : "");
        d.style.background = actionColors[t.action] || "#999";

        const widthPercent = (t.time / totalTime * 100);
        d.style.width = `calc(${widthPercent}% - 2px)`;
        d.style.minWidth = "30px";

        const nameSpan = document.createElement("span");
        nameSpan.textContent = t.name;
        d.appendChild(nameSpan);

        const timeSpan = document.createElement("span");
        timeSpan.className = "duration";
        timeSpan.textContent = `(${t.time})`;
        d.appendChild(timeSpan);

        d.title = `${t.name} - ${t.action} (${t.time} units)`;
        box.appendChild(d);
    });
}

// ===== Templates =====
const templates = {
    basic: `#include <stdio.h>
#include "uthread.h"

#define HIGH_PRIORITY 10
#define LOW_PRIORITY 1

void task(void *arg) {
    printf("Hello from a user thread!\\n");
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_create(task, NULL, HIGH_PRIORITY);
    uthread_start();
    return 0;
}`,
    priority: `#include <stdio.h>
#include "uthread.h"

// Demonstrates AGING: Low-priority threads eventually run.
#define HIGH_PRIORITY 20
#define LOW_PRIORITY 1

void worker(void *arg) {
    char *name = (char*)arg;
    for(int i=0; i<3; i++) {
        printf("[Thread %s] Priority-based execution\\n", name);
        for(volatile int j=0; j<80000000; j++); // Busy work
    }
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_create(worker, "High-P", HIGH_PRIORITY);
    uthread_create(worker, "Low-P", LOW_PRIORITY);
    uthread_start();
    return 0;
}`,
    preemption: `#include <stdio.h>
#include "uthread.h"

// Demonstrates PREEMPTION: High-P thread interrupts Low-P.
void slow_worker(void *arg) {
    printf("Low-P starting long task...\\n");
    for(volatile int j=0; j<500000000; j++); // Very long task
    printf("Low-P finished.\\n");
    uthread_exit();
}

void quick_higher_p(void *arg) {
    printf("--- HIGHER-P INTERRUPTED! ---\\n");
    uthread_exit();
}

int main() {
    uthread_init();
    // Low priority thread starts first
    uthread_create(slow_worker, NULL, 1);
    // Higher priority thread becomes ready
    uthread_create(quick_higher_p, NULL, 10);
    uthread_start();
    return 0;
}`,
    cooperative: `#include <stdio.h>
#include "uthread.h"

// Demonstrates COOPERATIVE multitasking using uthread_yield().
void item_processor(void *arg) {
    int id = *(int*)arg;
    for(int i=1; i<=3; i++) {
        printf("Thread %d processing item %d\\n", id, i);
        printf("Thread %d volunteering to YIELD...\\n", id);
        uthread_yield(); // Voluntarily give up CPU
    }
    uthread_exit();
}

int main() {
    uthread_init();
    int a=1, b=2;
    uthread_create(item_processor, &a, 5);
    uthread_create(item_processor, &b, 5);
    uthread_start();
    return 0;
}`,
    mutex: `#include <stdio.h>
#include "uthread.h"

uthread_mutex_t m;

void locked_task(void *arg) {
    uthread_mutex_lock(&m);
    printf("Thread %d got the mutex!\\n", *(int*)arg);
    for(volatile int j=0; j<100000000; j++); // Simulate work
    uthread_mutex_unlock(&m);
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_mutex_init(&m);
    int a=1, b=2;
    uthread_create(locked_task, &a, 1);
    uthread_create(locked_task, &b, 1);
    uthread_start();
    return 0;
}`
};

function loadTemplate(type) {
    if (templates[type]) {
        code.value = templates[type];
        updateLines();
    }
}

// ===== Dark/Light Theme =====
function toggleTheme() {
    document.body.classList.toggle("dark");
}
