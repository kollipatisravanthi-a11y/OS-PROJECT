from flask import Flask, request, jsonify, send_from_directory
import subprocess, os, uuid

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

def parse_gantt():
    if not os.path.exists(LOGFILE):
        return []

    lines = []
    with open(LOGFILE) as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) == 3:
                lines.append((int(parts[0]), parts[1], parts[2]))

    if not lines:
        return []

    events = []
    # Use the first timestamp as the base, or the "SYSTEM START" event if it exists
    base_time = lines[0][0]
    
    for i in range(len(lines) - 1):
        t_curr, name, action = lines[i]
        t_next, _, _ = lines[i+1]
        
        duration = t_next - t_curr
        if duration <= 0:
            duration = 1 # Minimal duration for simultaneous-looking events

        events.append({
            "name": name,
            "action": action,
            "time": duration
        })

    # The last transition doesn't have a next event to determine duration.
    # We'll give it a small proportional duration.
    last_t, last_name, last_action = lines[-1]
    events.append({
        "name": last_name,
        "action": last_action,
        "time": 1000 # Small constant for terminal event
    })

    return events

@app.route("/run", methods=["POST"])
def run():
    code = request.json["code"]
    src = os.path.join(TEMP, f"{uuid.uuid4()}.c")
    exe = src.replace(".c", "")

    with open(src, "w") as f:
        f.write(code)

    if os.path.exists(LOGFILE):
        os.remove(LOGFILE)

    import platform
    is_linux = platform.system() == "Linux"
    
    compile_cmd = ["gcc", src, UTHREAD, "-I", BASE, "-o", exe]
    if is_linux:
        compile_cmd.append("-lpthread")

    try:
        cp = subprocess.run(compile_cmd, capture_output=True)
        if cp.returncode != 0:
            return jsonify({"output": cp.stderr.decode(), "gantt": []})

        runp = subprocess.run(exe, capture_output=True, cwd=BASE, timeout=5)
        output = runp.stdout.decode()
    except FileNotFoundError:
        return jsonify({"output": "Error: GCC compiler not found in system PATH.", "gantt": []})
    except subprocess.TimeoutExpired as e:
        output = (e.stdout.decode() if e.stdout else "") + "\n[PROGRAM TIMED OUT AFTER 5s]"
    except Exception as e:
        return jsonify({"output": f"Unexpected Error: {str(e)}", "gantt": []})

    log_content = ""
    if os.path.exists(LOGFILE):
        with open(LOGFILE, "r") as f:
            log_content = f.read()

    return jsonify({
        "output": output,
        "gantt": parse_gantt(),
        "scheduler_log": log_content
    })

if __name__ == "__main__":
    app.run(debug=True)
