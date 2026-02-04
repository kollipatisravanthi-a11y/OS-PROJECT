from flask import Flask, request, jsonify, send_from_directory
import subprocess, os, uuid, shutil

app = Flask(__name__)

BASE = os.path.dirname(os.path.abspath(__file__))
TEMP = os.path.join(BASE, "temp")
FRONTEND = os.path.join(BASE, "../frontend")
UTHREAD = os.path.join(BASE, "uthread.c")
LOGFILE = os.path.join(BASE, "scheduler_log.txt")

os.makedirs(TEMP, exist_ok=True)

@app.route("/")
def home():
    return send_from_directory(FRONTEND, "index.html")

@app.route("/<path:path>")
def frontend_files(path):
    return send_from_directory(FRONTEND, path)

def parse_logs(log_path=LOGFILE):
    if not os.path.exists(log_path):
        return [], {}

    lines = []
    with open(log_path) as f:
        for line in f:
            parts = line.strip().split(' ', 2)
            if len(parts) >= 3:
                # Filter out SYSTEM events to unclutter Gantt chart
                if parts[1] == "SYSTEM":
                    continue
                lines.append((int(parts[0]), parts[1], parts[2]))

    if not lines:
        return [], {}

    events = []
    # Metrics & State Tracking
    thread_stats = {} 
    page_faults = 0
    disk_ios = 0
    
    # Deadlock detection structures
    resource_owners = {} # {sem_id: thread_name}
    wait_for = {}       # {thread_name: sem_id}
    deadlock_cycle = []

    for i in range(len(lines) - 1):
        t_curr, name, action = lines[i]
        t_next, _, _ = lines[i+1]
        
        duration = t_next - t_curr
        if duration < 0: duration = 0

        events.append({"name": name, "action": action, "time": duration})
        
        # Track Statistics & Deadlocks
        if "PAGE_FAULT" in action: page_faults += 1
        if "DISK_IO_START" in action: disk_ios += 1
        
        if "ACQUIRED_SEM" in action:
            sem_id = action.split()[-1]
            resource_owners[sem_id] = name
            if name in wait_for: del wait_for[name]
            
        if "BLOCKED_ON_SEM" in action:
            parts = action.split()
            sem_id = parts[-1].split('_')[0]
            wait_for[name] = sem_id
            
            # Simple Cycle Detection: T1 waits for S_ owned by T2, T2 waits for S_ owned by T1
            for t_name, s_id in wait_for.items():
                owner = resource_owners.get(s_id)
                if owner and wait_for.get(owner) == resource_owners.get(wait_for.get(t_name)):
                     # Basic potential deadlock detected
                     if name not in deadlock_cycle: deadlock_cycle.append(name)

        if "FINISHED" in action:
            # Cleanup for deadlock detection
            if name in wait_for: del wait_for[name]
            resource_owners = {k: v for k, v in resource_owners.items() if v != name}

        if name not in thread_stats and name != "SYSTEM":
            thread_stats[name] = {"created": t_curr, "finished": None, "wait_time": 0, "run_time": 0}
            
        if name in thread_stats:
            if "RUNNING" in action:
                thread_stats[name]["run_time"] += duration
            elif "CREATED" not in action and "FINISHED" not in action:
                thread_stats[name]["wait_time"] += duration
            
            if "FINISHED" in action:
                thread_stats[name]["finished"] = t_next

    # Last event
    last_t, last_name, last_action = lines[-1]
    events.append({
        "name": last_name,
        "action": last_action,
        "time": 1000 
    })

    # Finalize Metrics
    completed = [t for t in thread_stats.values() if t["finished"]]
    avg_wait = sum(t["wait_time"] for t in completed) / len(completed) if completed else 0
    avg_turnaround = sum(t["finished"] - t["created"] for t in completed) / len(completed) if completed else 0
    
    metrics = {
        "avg_wait": round(avg_wait / 1000, 2),
        "avg_turnaround": round(avg_turnaround / 1000, 2),
        "page_faults": page_faults,
        "disk_ios": disk_ios,
        "deadlock": len(deadlock_cycle) > 0,
        "throughput": len(completed)
    }

    return events, metrics

@app.route("/run", methods=["POST"])
def run():
    code = request.json["code"]
    
    # Create unique run directory for this request
    run_id = str(uuid.uuid4())
    run_dir = os.path.join(TEMP, run_id)
    os.makedirs(run_dir, exist_ok=True)
    
    src = os.path.join(run_dir, "program.c")
    exe = os.path.join(run_dir, "program")
    # Log file will be created in the CWD (run_dir)
    run_log = os.path.join(run_dir, "scheduler_log.txt")

    with open(src, "w") as f:
        f.write(code)

    import platform
    is_linux = platform.system() == "Linux"
    
    compile_cmd = ["gcc", src, UTHREAD, "-I", BASE, "-o", exe]
    if is_linux:
        compile_cmd.append("-lpthread")

    try:
        cp = subprocess.run(compile_cmd, capture_output=True)
        if cp.returncode != 0:
            shutil.rmtree(run_dir, ignore_errors=True)
            return jsonify({"output": cp.stderr.decode(), "gantt": [], "metrics": {}, "scheduler_log": ""})

        # Run with CWD = run_dir so scheduler_log.txt is written there
        runp = subprocess.run(exe, capture_output=True, cwd=run_dir, timeout=5)
        output = runp.stdout.decode()
    except FileNotFoundError:
        return jsonify({"output": "Error: GCC compiler not found in system PATH.", "gantt": [], "metrics": {}, "scheduler_log": ""})
    except subprocess.TimeoutExpired as e:
        output = (e.stdout.decode() if e.stdout else "") + "\n[PROGRAM TIMED OUT AFTER 5s]"
    except Exception as e:
        return jsonify({"output": f"Unexpected Error: {str(e)}", "gantt": [], "metrics": {}, "scheduler_log": ""})

    log_content = ""
    if os.path.exists(run_log):
        with open(run_log, "r") as f:
            log_content = f.read()

    # Pass the specific log file to parse_logs
    gantt, metrics = parse_logs(run_log)
    
    # Cleanup
    shutil.rmtree(run_dir, ignore_errors=True)

    return jsonify({
        "output": output,
        "gantt": gantt,
        "metrics": metrics,
        "scheduler_log": log_content
    })

if __name__ == "__main__":
    app.run(debug=True)
