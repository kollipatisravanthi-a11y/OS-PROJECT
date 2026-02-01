# Advanced Multi-Core Thread Visualizer 🚀

**Advanced Multi-Core Thread Visualizer** is a professional-grade OS simulator designed to visualize the internal mechanics of a User-Level Thread (ULT) library. It provides a real-time, interactive dashboard to observe scheduling decisions, preemption, and thread synchronization.

---

## 🌟 Key Features

- **Cross-Platform Scheduling Core:** Support for both **Windows Fibers** and **POSIX ucontext (Linux)**.
- **Dynamic Thread Dashboard:** Includes a real-time **Gantt Chart** and specialized log categories.
- **Sophisticated Scheduling:** Implements Priority-based scheduling with **Aging** and **Preemptive interrupts**.
- **Interactive Code Editor:** Write, compile, and run C code directly in the browser.
- **Educational Templates:** One-click demos for Preemption, Mutex locking, and Cooperative yielding.
- **Ready Queue Visibility:** Real-time logging of the Ready Queue state and thread priority changes.

---

## 🛠️ Tech Stack

- **Backend Logic:** C (Windows/Linux System APIs)
- **Server:** Python / Flask
- **Frontend:** Vanilla HTML5, CSS3, JavaScript
- **Code Highlighting:** PrismJS
- **Data Visualization:** Custom CSS Grid-based Gantt Chart

---

## 🚀 Getting Started

### Prerequisites

1.  **GCC Compiler:** 
    - **Windows:** Install [MinGW-w64](https://www.mingw-w64.org/).
    - **Linux:** `sudo apt install build-essential`
2.  **Python 3.9+**

### Installation

1.  Clone the repository:
    ```bash
    git clone https://github.com/yourusername/OS-Thread-Visualizer.git
    cd OS-Thread-Visualizer
    ```

2.  Install dependencies:
    ```bash
    pip install -r requirements.txt
    ```

3.  Run the application:
    ```bash
    python Backend/app.py
    ```

4.  Open your browser and navigate to `http://127.0.0.1:5000`.

---

## 📊 How It Works

The system utilizes a hybrid architecture:
1.  **The C Core:** Manages the Thread Control Blocks (TCB), handles context switching using system-level primitives, and logs microsecond-level events to a local file.
2.  **The Python Flask API:** Receives C code from the UI, executes the compiler, manages process timeouts (5s safety), and parses the raw logs into JSON.
3.  **The JS Interface:** Dynamically renders the Gantt chart and distributes logs into "Scheduler", "Thread Output", and "Aging/Queue" tabs for analysis.

---

## 🛡️ Educational Demos

- **Priority (Aging):** Observe how low-priority threads are boosted to prevent starvation.
- **Preemption:** Witness a high-priority interrupt "stop" a running thread in its tracks.
- **Mutex Demo:** See how lock contention affects the Gantt chart and thread states.

---

## 📜 License

This project was developed for educational purposes as part of an Operating Systems final project.

---

*“Visualizing the heartbeat of the Operating System.”*
