const code = document.getElementById("code");
const lines = document.getElementById("lineNumbers");
const highlight = document.getElementById("highlight");

// ===== Editor Logic =====
function updateLines() {
    const count = code.value.split("\n").length;
    lines.textContent = "";
    for (let i = 1; i <= count; i++) {
        const span = document.createElement("div");
        span.textContent = i;
        lines.appendChild(span);
    }
    updateHighlight();
}

function updateHighlight() {
    highlight.innerHTML = Prism.highlight(code.value, Prism.languages.c, 'c');
    highlight.scrollTop = code.scrollTop;
}

code.addEventListener("input", updateLines);
code.addEventListener("scroll", () => {
    lines.scrollTop = code.scrollTop;
    highlight.scrollTop = code.scrollTop;
});

// ===== Kernel Actions =====
function run() {
    const runBtn = document.getElementById("runBtn");
    runBtn.disabled = true;
    runBtn.textContent = "Running...";

    fetch("/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code: code.value })
    })
        .then(res => res.json())
        .then(data => {
            runBtn.disabled = false;
            runBtn.textContent = "▶ Execute Kernel";

            if (data.metrics) updateUI(data.metrics, data.scheduler_log);
            if (data.gantt) drawGantt(data.gantt);

            document.getElementById("threadOutput").textContent = data.output;
        })
        .catch(err => {
            runBtn.disabled = false;
            runBtn.textContent = "▶ Execute Kernel Trace";
            alert("Kernel Panic: " + err);
        });
}

function updateUI(m, log) {
    // Default log to empty string if undefined to prevent split errors
    log = log || "";

    // Metrics
    document.getElementById("metric-wait").textContent = m.avg_wait + "ms";
    document.getElementById("metric-turnaround").textContent = m.avg_turnaround + "ms";
    document.getElementById("metric-throughput").textContent = m.throughput;
    document.getElementById("metric-faults").textContent = m.page_faults;

    // Deadlock Alert
    const lockStatus = document.getElementById("lockStatus");
    if (m.deadlock) {
        lockStatus.innerHTML = '<span style="color:var(--neon-red); font-weight:bold;">⚠️ DEADLOCK DETECTED! Circular wait identified in Resource Graph.</span>';
    } else {
        lockStatus.textContent = "All resources operational. No circular dependencies.";
    }

    // Logs & Visuals
    const scheduler = document.getElementById("schedulerLog");
    const aging = document.getElementById("agingLog");
    scheduler.textContent = "";
    aging.textContent = "";

    let lastMLFQ = "";
    let ioThreads = [];
    let memoryMappings = [];

    log.split('\n').forEach(line => {
        if (!line.trim()) return;
        const parts = line.split(' ');
        const thread = parts[1];
        const action = parts.slice(2).join(' ');

        if (action.includes("MLFQ:")) lastMLFQ = action;
        if (action.includes("DISK_IO_START")) ioThreads.push(thread);
        if (action.includes("DISK_IO_DONE")) ioThreads = ioThreads.filter(t => t !== thread);

        if (action.includes("PAGE_FAULT_MAPPED")) {
            const pPage = action.split("->P:")[1];
            memoryMappings.push({ thread, pPage });
        }

        if (action.includes("MLFQ") || action.includes("SYSTEM")) {
            aging.textContent += `[KERN] ${thread}: ${action}\n`;
        } else {
            scheduler.textContent += `[TASK] ${thread}: ${action}\n`;
        }
    });

    drawMLFQ(lastMLFQ);
    drawIO(ioThreads);
    drawMemoryMap(memoryMappings);
}

function drawMLFQ(mlfqStr) {
    ["q0", "q1", "q2"].forEach(q => {
        const div = document.getElementById(q);
        const label = q.startsWith("q0") ? "Q0 (RR)" : q.startsWith("q1") ? "Q1 (RR)" : "Q2 (FCFS)";
        div.innerHTML = `<div class="queue-label">${label}</div>`;

        const match = mlfqStr.match(new RegExp(`${q.toUpperCase()}\\[(.*?)\\]`));
        if (match && match[1].trim()) {
            match[1].trim().split(' ').forEach(t => {
                const token = document.createElement("div");
                token.className = "thread-token";
                token.textContent = t;
                div.appendChild(token);
            });
        }
    });
}

function drawIO(threads) {
    const div = document.getElementById("io_wait");
    div.innerHTML = '<div class="queue-label">DISK</div>';
    threads.forEach(t => {
        const token = document.createElement("div");
        token.className = "thread-token";
        token.style.background = "var(--neon-orange)";
        token.textContent = t;
        div.appendChild(token);
    });
}

function drawMemoryMap(mappings) {
    const grid = document.getElementById("physicalMemory");
    grid.innerHTML = "";
    for (let i = 0; i < 8; i++) {
        const page = document.createElement("div");
        page.className = "memory-page";
        const owner = mappings.find(m => parseInt(m.pPage) === i);
        if (owner) {
            page.classList.add("active");
            page.innerHTML = `<span style="color:var(--neon-blue)">PAGE ${i}</span><span style="font-size:10px">${owner.thread}</span>`;
        } else {
            page.innerHTML = `<span style="opacity:0.3">PAGE ${i}</span><span style="font-size:8px; opacity:0.1">EMPTY</span>`;
        }
        grid.appendChild(page);
    }
}

function drawGantt(gantt) {
    const box = document.getElementById("gantt");
    box.innerHTML = "";
    const totalTime = gantt.reduce((sum, t) => sum + t.time, 0);

    gantt.forEach(t => {
        const d = document.createElement("div");
        d.className = "block";
        const widthPercent = (t.time / totalTime * 100);
        d.style.width = `calc(${widthPercent}% - 3px)`;
        d.style.minWidth = "20px";

        // Dynamic Coloring
        if (t.action.includes("RUNNING")) d.style.background = "#16a34a";
        else if (t.action.includes("MLFQ_DOWNGRADE")) d.style.background = "#dc2626";
        else if (t.action.includes("DISK_IO")) d.style.background = "#ea580c";
        else if (t.action.includes("BLOCKED")) d.style.background = "#9333ea";
        else if (t.action.includes("UNBLOCKED")) d.style.background = "#2563eb";
        else d.style.background = "#777";

        const name = document.createElement("span");
        name.textContent = t.name;
        d.appendChild(name);

        const timeMs = (t.time / 1000).toFixed(3);
        d.title = `${t.name}: ${t.action} (${timeMs}ms)`;
        d.onclick = () => alert(`Thread: ${t.name}\nAction: ${t.action}\nDuration: ${timeMs}ms`);
        box.appendChild(d);
    });
}

// ===== Templates =====
const templates = {
    mlfq: `#include <stdio.h>
#include "uthread.h"

void worker(void *arg) {
    for(int i=0; i<3; i++) {
        printf("%s working...\\n", (char*)arg);
        for(volatile int j=0; j<80000000; j++); // Busy work
    }
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_create(worker, "LongTask", 0); // Will downgrade
    uthread_create(worker, "QuickTask", 0);
    uthread_start();
    return 0;
}`,
    paging: `#include <stdio.h>
#include "uthread.h"

void loader(void *arg) {
    printf("Requesting 4KB pages...\\n");
    uthread_malloc(4096);
    uthread_malloc(4096);
    for(volatile int j=0; j<50000000; j++);
    uthread_exit();
}

int main() {
    uthread_init();
    for(int i=0; i<5; i++) uthread_create(loader, NULL, 0);
    uthread_start();
    return 0;
}`,
    prod_cons: `#include <stdio.h>
#include "uthread.h"

uthread_sem_t mutex, full, empty;

void producer(void *arg) {
    uthread_sem_wait(&empty);
    uthread_sem_wait(&mutex);
    printf("Item Produced.\\n");
    uthread_sem_post(&mutex);
    uthread_sem_post(&full);
    uthread_exit();
}

void consumer(void *arg) {
    uthread_sem_wait(&full);
    uthread_sem_wait(&mutex);
    printf("Item Consumed.\\n");
    uthread_sem_post(&mutex);
    uthread_sem_post(&empty);
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_sem_init(&mutex, 1);
    uthread_sem_init(&full, 0);
    uthread_sem_init(&empty, 1);
    uthread_create(producer, NULL, 1);
    uthread_create(consumer, NULL, 1);
    uthread_start();
    return 0;
}`,
    deadlock: `#include <stdio.h>
#include "uthread.h"

uthread_sem_t s1, s2;

void task_a(void *arg) {
    uthread_sem_wait(&s1);
    for(volatile int j=0; j<50000000; j++); 
    uthread_sem_wait(&s2); // Circular Wait
    uthread_exit();
}

void task_b(void *arg) {
    uthread_sem_wait(&s2);
    for(volatile int j=0; j<50000000; j++);
    uthread_sem_wait(&s1); // Circular Wait
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_sem_init(&s1, 1);
    uthread_sem_init(&s2, 1);
    uthread_create(task_a, NULL, 0);
    uthread_create(task_b, NULL, 0);
    uthread_start();
    return 0;
}`,
    disk_io: `#include <stdio.h>
#include "uthread.h"

void io_task(void *arg) {
    printf("Requesting Disk block 102...\\n");
    uthread_disk_io(102); // Blocks thread
    printf("Disk Data Loaded!\\n");
    uthread_exit();
}

int main() {
    uthread_init();
    uthread_create(io_task, NULL, 0);
    uthread_create(io_task, NULL, 0);
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

function reset() { location.reload(); }
function toggleTheme() { document.body.classList.toggle("dark"); }
function showTab(t) {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    event.currentTarget.classList.add('active');
    document.getElementById(t + 'Tab').classList.add('active');
}

// Init
loadTemplate('mlfq');
drawMemoryMap([]);
