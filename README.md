# User Level Thread Library: Advanced OS Simulation 🚀

**User Level Thread Library** is a high-fidelity Operating System simulator designed to visualize the internal mechanics of a modern kernel. Beyond basic thread management, this version implements sophisticated subsystems including **MLFQ Scheduling**, **Virtual Memory MMU**, and **Resource Graphing**.

---

## 🌟 Advanced OS Features

- **Multi-Level Feedback Queue (MLFQ):** A dynamic scheduler that manages 3 levels of queues (Q0, Q1, Q2). Threads are automatically downgraded based on CPU quantum consumption and periodically boosted to prevent starvation.
- **Virtual Memory & Paging MMU:** Simulates a hardware-level Memory Management Unit. Observe **Page Tables** mapping virtual addresses to **Physical RAM** slots, and watch **Page Faults** trigger in real-time.
- **True Blocking Synchronization:** Implements semaphores with a blocked-queue state. Unlike basic spin-locks, threads are completely removed from the CPU while waiting, optimizing system throughput.
- **Disk I/O Subsystem:** Simulates asynchronous I/O operations. Threads can enter a `DISK_WAIT` state, allowing the scheduler to run other tasks while "hardware" I/O completes.
- **Resource Graph & Deadlock Detection:** Real-time tracking of semaphore ownership. The system can identify circular wait patterns and alert the user to potential deadlocks.
- **Executive Metrics Dashboard:** Real-time tracking of:
    - **Avg Wait Time:** Measures ready-queue latency.
    - **Avg Turnaround:** Tracks the full thread lifecycle.
    - **Page Fault Count:** Monitors memory pressure and mapping efficiency.

---

## 🛠️ Tech Stack

- **Kernel Logic:** C (Windows Fibers / POSIX ucontext)
- **Middleware:** Python 3.9+ / Flask
- **Display Engine:** Vanilla HTML5 / Modern CSS / JavaScript
- **Instrumentation:** High-precision telemetry logging (Microsecond accuracy)

---

## 🚀 Getting Started

### Prerequisites

1.  **GCC Compiler:** 
    - **Windows:** Install [MinGW-w64](https://www.mingw-w64.org/).
    - **Linux:** `sudo apt install build-essential`
2.  **Python 3.9+**

### Installation

1.  Clone the repository and install requirements:
    ```bash
    pip install -r requirements.txt
    ```

2.  Run the application:
    ```bash
    python Backend/app.py
    ```

3.  Open your browser and navigate to `http://127.0.0.1:5000`.

---

## 📊 System Architecture

The simulation utilizes a 3-layer architecture:
1.  **The C Kernel:** Manages TCBs, context switching, MLFQ logic, and MMU paging. It logs all "Kernel Events" to a telemetry file.
2.  **The Data Processor (Python):** Compiles user code on-the-fly, executes the binary, and performs post-run analytics to calculate wait times and detect deadlocks from the logs.
3.  **The Visualizer (JS):** Animates the internal state including the **Gantt Chart**, **MLFQ Queues**, and **Physical Memory Map**.

---

## 📜 Educational Scenarios

Navigate to the **Library API** sidebar to load specialized templates:
- **MLFQ Scheduling:** Watch the priority downgrade logic in action.
- **Paging Demo:** Observe how threads occupy physical RAM pages.
- **Producer/Consumer:** A classic synchronization puzzle with true blocking semaphores.
- **Deadlock Scenario:** Intentionally create a circular wait to see the detection system trigger.

---

*“Visualizing the complex heartbeat of modern Operating Systems.”*
