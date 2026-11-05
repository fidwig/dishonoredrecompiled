#!/usr/bin/env python3
"""
Antigravity & Ninja Real-Time Build Monitor v2
Enhanced High-Performance Edition for macOS Apple Silicon
Run in any terminal window:
    python3 .build_monitoring/monitor_build_v2.py
"""

import os
import re
import sys
import time
import glob
import signal
import subprocess
from collections import deque

# ANSI Color & Style Codes
BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"
CLEAR_SCREEN = "\033[H\033[J"
CLEAR_LINE = "\033[2K"

# Colors
BLACK = "\033[30m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
MAGENTA = "\033[35m"
CYAN = "\033[36m"
WHITE = "\033[37m"

# Regular Expressions
NINJA_STEP_RE = re.compile(r"\[(\d+)/(\d+)\]\s+(.*)")

def get_terminal_width(default=92):
    try:
        return os.get_terminal_size().columns
    except Exception:
        return default

def format_duration(seconds):
    """Format duration into friendly MM:SS or HH:MM:SS."""
    if seconds is None or seconds < 0:
        return "--:--"
    m, s = divmod(int(seconds), 60)
    h, m = divmod(m, 60)
    if h > 0:
        return f"{h:02d}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"

def format_target_duration(seconds):
    """Format file compilation duration into clean, human-friendly seconds/minutes."""
    if seconds is None:
        return "done"
    if seconds >= 60:
        m, s = divmod(int(round(seconds)), 60)
        return f"{m}m {s:02d}s"
    if seconds >= 10:
        return f"{int(round(seconds))}s"
    if seconds < 0.1:
        return "<0.1s"
    return f"{seconds:.1f}s"

def make_progress_bar(current, total, width=32):
    """Generate a clean visual block progress bar."""
    if total <= 0:
        return "[" + " " * width + "] 0.0%"
    pct = min(1.0, max(0.0, current / total))
    filled = int(round(width * pct))
    bar = "█" * filled + "░" * (width - filled)
    return f"[{bar}] {pct * 100:5.1f}%"

def is_process_running(proc_name):
    """Check if a specific process is active."""
    try:
        res = subprocess.run(["pgrep", "-x", proc_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=0.3)
        return res.returncode == 0
    except Exception:
        return False

def count_clang_processes():
    """Counts active clang / clang++ compiler processes reliably via pgrep."""
    try:
        res = subprocess.run(["pgrep", "-f", r"(clang\+\+|clang) "], capture_output=True, text=True, timeout=0.3)
        pids = [p for p in res.stdout.strip().split("\n") if p]
        return len(pids)
    except Exception:
        return 0

def get_active_clang_processes():
    """Inspect active clang++ command lines via ps -ww -eo pid,%cpu,etime,command.
    Uses -ww so 1,500+ char command lines with -I / -D flags don't truncate -c <file.cpp>."""
    active = []
    try:
        cmd = ["ps", "-ww", "-eo", "pid,%cpu,etime,command"]
        res = subprocess.run(cmd, capture_output=True, text=True, errors="ignore", timeout=0.5)
        if res.returncode != 0:
            return []
        
        for line in res.stdout.strip().splitlines():
            line = line.strip()
            if not line:
                continue
            parts = line.split(None, 3)
            if len(parts) < 4:
                continue
            pid_str, cpu_str, etime_str, command_str = parts[0], parts[1], parts[2], parts[3]
            
            # Match clang or clang++ compilation commands
            if ("clang++" in command_str or "clang" in command_str) and " -c " in command_str:
                source_file = "compiling..."
                tokens = command_str.split()
                if "-c" in tokens:
                    idx = tokens.index("-c")
                    if idx + 1 < len(tokens):
                        source_file = tokens[idx + 1]
                else:
                    for t in reversed(tokens):
                        if t.endswith((".cpp", ".c", ".cc", ".m", ".mm")) and not t.startswith("-"):
                            source_file = t
                            break
                
                module = "Engine"
                if "Core" in source_file:
                    module = "Core"
                elif "GameFramework" in source_file:
                    module = "GameFramework"
                elif "IpDrv" in source_file:
                    module = "IpDrv"
                elif "GFx" in source_file or "gfx" in source_file:
                    module = "GFx3"
                elif "Launch" in source_file or "DishonoredGame" in source_file:
                    module = "Dishonored"
                elif "zlib" in source_file or "libpng" in source_file or "lzokay" in source_file:
                    module = "ThirdParty"
                
                active.append({
                    "pid": pid_str,
                    "cpu": cpu_str,
                    "etime": etime_str,
                    "source": os.path.basename(source_file),
                    "full_source": source_file,
                    "module": module,
                })
    except Exception:
        pass
    return active

def find_ninja_log():
    """Dynamically locates .ninja_log regardless of current working directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.normpath(os.path.join(script_dir, "..", "build", "darwin-debug", ".ninja_log")),
        os.path.normpath(os.path.join(script_dir, "..", "build", ".ninja_log")),
        os.path.join(os.getcwd(), "build", "darwin-debug", ".ninja_log"),
        os.path.join(os.getcwd(), "build", ".ninja_log"),
        "/Users/jsph/Documents/dev/dmisc/dishonoredrecompiled/build/darwin-debug/.ninja_log",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    found = glob.glob("**/build/**/.ninja_log", recursive=True)
    if found:
        return found[0]
    return None

def parse_ninja_log_durations(log_path):
    """Parse .ninja_log exclusively for target duration lookup map."""
    duration_map = {}
    if not log_path or not os.path.exists(log_path):
        return duration_map

    try:
        with open(log_path, "r", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) >= 4:
                    start_ms = int(parts[0])
                    end_ms = int(parts[1])
                    target = parts[3]
                    duration_s = max(0.01, (end_ms - start_ms) / 1000.0)
                    target_name = os.path.basename(target)
                    duration_map[target_name] = duration_s
                    duration_map[target] = duration_s
    except Exception:
        pass

    return duration_map

def find_active_log_file():
    """Find the most recent task log from ~/.gemini/antigravity-cli or local dir."""
    candidates = []
    # 1. Direct path to current conversation tasks
    candidates.extend(glob.glob("/Users/jsph/.gemini/antigravity-cli/brain/313901c1-d550-4cc4-b242-8d0c7587a60f/.system_generated/tasks/task-*.log"))
    # 2. Local build tasks if any
    if not candidates:
        candidates.extend(glob.glob("build/**/task-*.log", recursive=True))
    # 3. General brain tasks wildcard fallback
    if not candidates:
        candidates.extend(glob.glob(os.path.expanduser("~/.gemini/antigravity-cli/brain/*/.system_generated/tasks/task-*.log")))

    if not candidates:
        return None
    candidates.sort(key=lambda p: os.path.getmtime(p), reverse=True)
    return candidates[0]

def prime_from_task_log(filepath, count=7):
    """Fast reverse chunk seeker that inspects the end of task_log to instantly
    prime current_step, total_steps, current_action, recent_completed, and completion state in <1ms."""
    if not filepath or not os.path.exists(filepath):
        return 0, 0, "Standby", [], False
    try:
        size = os.path.getsize(filepath)
        chunk_size = 128 * 1024
        pos = size
        found = []
        leftover = ""
        latest_step = 0
        latest_total = 0
        latest_action = "Standby"
        has_completion_marker = False
        
        with open(filepath, "r", errors="ignore") as f:
            while pos > 0 and (len(found) < count or not has_completion_marker):
                read_size = min(pos, chunk_size)
                pos -= read_size
                f.seek(pos)
                chunk = f.read(read_size) + leftover
                lines = chunk.splitlines()
                if pos > 0:
                    leftover = lines[0]
                    lines = lines[1:]
                for line in reversed(lines):
                    if not has_completion_marker and any(marker in line for marker in [
                        "ninja: no work to do",
                        "Linking CXX executable Binaries/Mac/DishonoredGame",
                        "finished with result:",
                        "Command exited with code 0"
                    ]):
                        has_completion_marker = True

                    if line.startswith("[") and "]" in line and len(found) < count:
                        m = NINJA_STEP_RE.search(line)
                        if m:
                            step = int(m.group(1))
                            total = int(m.group(2))
                            act = m.group(3).strip()
                            if not latest_step:
                                latest_step = step
                                latest_total = total
                                latest_action = act
                            target_raw = act.split()[-1] if act else ""
                            target_name = os.path.basename(target_raw)
                            if target_name:
                                mod = "Engine"
                                if "Core" in target_raw:
                                    mod = "Core"
                                elif "GameFramework" in target_raw:
                                    mod = "GameFramework"
                                elif "IpDrv" in target_raw:
                                    mod = "IpDrv"
                                elif "GFx" in target_raw or "gfx" in target_raw:
                                    mod = "GFx3"
                                elif "Dishonored" in target_raw:
                                    mod = "DishonoredGame"
                                elif "_deps" in target_raw or "zlib" in target_raw or "png" in target_raw:
                                    mod = "ThirdParty"
                                found.append({
                                    "name": target_name,
                                    "raw": target_raw,
                                    "module": mod,
                                    "duration_s": None
                                })
                if latest_step > 0 and (len(found) >= count or (size - pos) > 1024 * 1024):
                    break
                if size - pos > 4 * 1024 * 1024:
                    break
        found.reverse()
        return latest_step, latest_total, latest_action, found, has_completion_marker
    except Exception:
        return 0, 0, "Standby", [], False

def read_task_log_updates(task_log, last_pos):
    """Fast streaming reader that only reads new bytes appended since last_pos."""
    if not task_log or not os.path.exists(task_log):
        return [], 0
    try:
        size = os.path.getsize(task_log)
        if last_pos >= size:
            return [], last_pos
        
        with open(task_log, "r", errors="ignore") as f:
            f.seek(last_pos)
            lines = f.readlines()
            new_pos = f.tell()
            return lines, new_pos
    except Exception:
        return [], last_pos

def get_system_metrics():
    """Returns load average and RAM estimate without external pip packages."""
    load_1, load_5, load_15 = os.getloadavg()
    mem_total_gb = 16.0
    try:
        res = subprocess.run(["sysctl", "-n", "hw.memsize"], capture_output=True, text=True, timeout=0.3)
        if res.returncode == 0:
            mem_total_gb = int(res.stdout.strip()) / (1024 ** 3)
    except Exception:
        pass
    return f"{load_1:.1f}, {load_5:.1f}, {load_15:.1f}", f"{mem_total_gb:.0f} GB"

def main():
    def cleanup_and_exit(signum, frame):
        sys.stdout.write("\033[?25h\n")
        sys.stdout.flush()
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup_and_exit)
    signal.signal(signal.SIGTERM, cleanup_and_exit)

    # Hide cursor
    sys.stdout.write("\033[?25l")
    sys.stdout.flush()

    last_log_path = None
    last_task_pos = 0
    current_step = 0
    total_steps = 0
    current_action = "Standby"
    build_start_time = None
    task_mtime = None
    task_log_completed = False
    is_build_completed = False

    # Rolling history of completed targets strictly from the live stream
    recent_completed = deque(maxlen=7)
    current_module_counts = {}

    # Lightweight diagnostics buffers
    recent_warnings = deque(maxlen=3)
    recent_errors = deque(maxlen=3)
    warning_count = 0
    error_count = 0

    # Rate & ETA calculation
    throughput_samples = deque(maxlen=30)
    
    spinner_idx = 0
    spinners = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

    # Cached ninja durations map (updated on mtime change, non-blocking)
    ninja_log_path = find_ninja_log()
    last_ninja_mtime = 0
    duration_map = {}
    if ninja_log_path and os.path.exists(ninja_log_path):
        duration_map = parse_ninja_log_durations(ninja_log_path)

    try:
        while True:
            now = time.time()
            spinner_idx = (spinner_idx + 1) % len(spinners)
            spinner = spinners[spinner_idx]

            # 1. Locate active task log & establish accurate build start time
            task_log = find_active_log_file()
            if task_log != last_log_path:
                last_log_path = task_log
                last_task_pos = os.path.getsize(task_log) if task_log and os.path.exists(task_log) else 0
                recent_warnings.clear()
                recent_errors.clear()
                recent_completed.clear()
                current_module_counts.clear()
                warning_count = 0
                error_count = 0
                throughput_samples.clear()
                task_log_completed = False
                is_build_completed = False

                # Instantly prime state from end of log in <1ms without freezing
                p_step, p_total, p_action, p_items, p_completed = prime_from_task_log(task_log, 7)
                task_log_completed = p_completed
                if p_total > 0:
                    current_step = p_step
                    total_steps = p_total
                    current_action = p_action
                    for it in p_items:
                        dur = duration_map.get(it["name"], duration_map.get(it.get("raw", ""), None))
                        it["duration_s"] = dur
                        recent_completed.append(it)
                        current_module_counts[it["module"]] = current_module_counts.get(it["module"], 0) + 1

            # Check task log file timestamps for exact elapsed time calculation
            log_active = False
            if task_log and os.path.exists(task_log):
                try:
                    st = os.stat(task_log)
                    task_mtime = st.st_mtime
                    build_start_time = getattr(st, "st_birthtime", st.st_ctime)
                    log_active = (now - task_mtime) < 4.0 and not task_log_completed
                except Exception:
                    pass

            # 2. Active Clang Workers Detection
            pgrep_clang_count = count_clang_processes()
            active_clangs = get_active_clang_processes()
            clang_count = max(pgrep_clang_count, len(active_clangs))
            ninja_running = is_process_running("ninja")

            # Active build state: processes running or log written <4s ago
            if ninja_running or clang_count > 0:
                is_building = True
                is_build_completed = False
            elif log_active and not task_log_completed:
                is_building = True
                is_build_completed = False
            else:
                is_building = False

            if build_start_time is None and is_building:
                build_start_time = now

            # 3. Read .ninja_log durations on mtime change (cached & non-blocking)
            if ninja_log_path and os.path.exists(ninja_log_path):
                try:
                    nmtime = os.path.getmtime(ninja_log_path)
                    if nmtime != last_ninja_mtime:
                        last_ninja_mtime = nmtime
                        duration_map = parse_ninja_log_durations(ninja_log_path)
                except Exception:
                    pass

            # 4. Fast incremental tail read from active task log (never freezes!)
            new_lines, last_task_pos = read_task_log_updates(task_log, last_task_pos)
            
            # Fast substring search for warnings/errors (no heavy regexes on 20,000 lines)
            for line in new_lines:
                if " warning:" in line:
                    warning_count += 1
                elif " error:" in line or line.startswith("FAILED:"):
                    error_count += 1

                if any(marker in line for marker in [
                    "ninja: no work to do",
                    "Linking CXX executable Binaries/Mac/DishonoredGame",
                    "finished with result:",
                    "Command exited with code 0"
                ]):
                    task_log_completed = True

                # Parse live completed step strictly from stdout stream
                if line.startswith("[") and "]" in line:
                    match = NINJA_STEP_RE.search(line)
                    if match:
                        step = int(match.group(1))
                        total = int(match.group(2))
                        action = match.group(3).strip()

                        # Reset if new build started in same file
                        if step < current_step:
                            recent_completed.clear()
                            current_module_counts.clear()
                            throughput_samples.clear()
                            is_build_completed = False
                            task_log_completed = False

                        current_step = step
                        total_steps = total
                        current_action = action

                        # Extract target and module strictly from live stream
                        target_raw = action.split()[-1] if action else ""
                        target_name = os.path.basename(target_raw)
                        if target_name:
                            mod = "Engine"
                            if "Core" in target_raw:
                                mod = "Core"
                            elif "GameFramework" in target_raw:
                                mod = "GameFramework"
                            elif "IpDrv" in target_raw:
                                mod = "IpDrv"
                            elif "GFx" in target_raw or "gfx" in target_raw:
                                mod = "GFx3"
                            elif "Dishonored" in target_raw:
                                mod = "DishonoredGame"
                            elif "_deps" in target_raw or "zlib" in target_raw or "png" in target_raw:
                                mod = "ThirdParty"

                            current_module_counts[mod] = current_module_counts.get(mod, 0) + 1
                            dur = duration_map.get(target_name, duration_map.get(target_raw, None))

                            recent_completed.append({
                                "name": target_name,
                                "raw": target_raw,
                                "module": mod,
                                "duration_s": dur
                            })

            # Sample warning/error snippet strictly from the last 20 lines (instant)
            if new_lines:
                for line in new_lines[-20:]:
                    sline = line.strip()
                    if " warning:" in sline and sline not in recent_warnings:
                        recent_warnings.append(sline)
                    elif (" error:" in sline or sline.startswith("FAILED:")) and sline not in recent_errors:
                        recent_errors.append(sline)

            # 5. Throughput & ETA Calculation
            throughput_samples.append((now, current_step))
            rate = 0.0
            if len(throughput_samples) >= 2:
                oldest_t, oldest_step = throughput_samples[0]
                dt = now - oldest_t
                dstep = current_step - oldest_step
                if dt > 1.0 and dstep >= 0:
                    rate = dstep / dt

            eta_seconds = None
            if rate > 0.05 and total_steps > current_step:
                eta_seconds = (total_steps - current_step) / rate

            # Robust Build Completion Detection
            if not is_building and total_steps > 0:
                cond_a = (current_step >= total_steps)
                cond_b = ("Linking CXX executable" in current_action or "DishonoredGame" in current_action) and (current_step >= max(1, total_steps - 10))
                cond_c = (current_step >= max(1, total_steps - 5))
                cond_d = task_log_completed

                if cond_a or cond_b or cond_c or cond_d:
                    is_build_completed = True

            # 6. Determine Current Phase
            phase = "STANDBY"
            if is_build_completed:
                phase = "COMPLETED"
            elif is_building:
                if "DishonoredGame" in current_action and ("LINK" in current_action or "clang++" in current_action):
                    phase = "LINKING EXECUTABLE (DishonoredGame)"
                elif "libEngine.a" in current_action or "Engine" in current_action:
                    phase = "COMPILING ENGINE (libEngine.a)"
                elif "libCore.a" in current_action or "Core" in current_action:
                    phase = "COMPILING CORE (libCore.a)"
                elif "libGameFramework.a" in current_action or "GameFramework" in current_action:
                    phase = "COMPILING GAMEFRAMEWORK"
                elif "libIpDrv.a" in current_action or "IpDrv" in current_action:
                    phase = "COMPILING IPDRV NETWORKING"
                elif "libgfx3.a" in current_action or "GFx" in current_action:
                    phase = "COMPILING GFX3 SCALEFORM"
                elif "cmake" in current_action.lower():
                    phase = "CMAKE CONFIGURATION"
                else:
                    phase = "COMPILING C++ UNITS"
            else:
                phase = "IDLE / WAITING"

            # 7. Render Terminal UI
            width = max(84, min(120, get_terminal_width()))
            line_sep = "─" * (width - 2)
            double_sep = "═" * (width - 2)

            out = []
            out.append(CLEAR_SCREEN)
            title_text = " Dishonored macOS Native Build Monitor v2 (Apple Silicon ARM64)"
            pad_len = max(0, (width - 2) - len(title_text))
            title_line = f"{CYAN}{BOLD}║{RESET} {BOLD}Dishonored macOS Native Build Monitor v2{RESET} {DIM}(Apple Silicon ARM64){RESET}{' ' * pad_len}{CYAN}{BOLD}║{RESET}"
            out.append(f"{CYAN}{BOLD}╔{double_sep}╗{RESET}")
            out.append(title_line)
            out.append(f"{CYAN}{BOLD}╚{double_sep}╝{RESET}")

            # Status & Phase Header
            if is_build_completed:
                status_str = f"{GREEN}{BOLD}✔ BUILD COMPLETE & LINKED{RESET}"
            elif is_building:
                status_str = f"{YELLOW}{BOLD}{spinner} BUILDING{RESET} ({clang_count} active clang threads)"
            elif error_count > 0:
                status_str = f"{RED}{BOLD}✖ FAILED{RESET}"
            else:
                status_str = f"{DIM}💤 IDLE (Ready for next build task){RESET}"

            out.append(f" {BOLD}Status:{RESET}  {status_str}")
            out.append(f" {BOLD}Phase:{RESET}   {MAGENTA}{BOLD}{phase}{RESET}")

            # Performance & System Metrics
            load_str, ram_str = get_system_metrics()
            if is_build_completed:
                rate_str = "done"
                eta_str = "00:00"
            else:
                rate_str = f"{rate:4.1f} files/sec" if rate > 0 else "evaluating..."
                eta_str = format_duration(eta_seconds) if eta_seconds else "calculating..."
            
            # Freeze elapsed time strictly at task_mtime - build_start_time when not is_building
            if is_building:
                elapsed_str = format_duration(now - build_start_time) if build_start_time else "--:--"
            elif build_start_time and task_mtime:
                elapsed_str = format_duration(max(0, task_mtime - build_start_time))
            else:
                elapsed_str = "--:--"

            out.append(f" {BOLD}Metrics:{RESET} Rate: {GREEN}{rate_str}{RESET} │ ETA: {CYAN}{eta_str}{RESET} │ Elapsed: {elapsed_str} │ Load: {DIM}{load_str}{RESET}")
            out.append(f"{DIM}┌{line_sep}┐{RESET}")

            # Progress Bar Section (strictly current_step out of total_steps, forced to 100% on completed)
            if total_steps > 0:
                display_step = total_steps if is_build_completed else current_step
                bar = make_progress_bar(display_step, total_steps, width=int(width * 0.42))
                out.append(f"  Progress: {GREEN}{BOLD}{bar}{RESET}  ({display_step} / {total_steps} targets)")
            else:
                out.append(f"  Progress: {DIM}[░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] Standby{RESET}")

            # Module Breakdown Pills for Current Build
            if current_module_counts:
                pills = []
                for mod, cnt in sorted(current_module_counts.items(), key=lambda x: -x[1]):
                    pills.append(f"{CYAN}{mod}:{RESET}{BOLD}{cnt}{RESET}")
                out.append(f"  Modules:  {' │ '.join(pills[:6])}")

            out.append(f"{DIM}├{line_sep}┤{RESET}")

            # Multi-Core Live Clang Tracking Panel
            out.append(f" {BOLD}Live Clang++ File Tracking across CPU Cores ({clang_count} active workers):{RESET}")
            if active_clangs:
                for i, c in enumerate(active_clangs[:10]):
                    prefix = f"  {YELLOW}▶{RESET} [Core {i+1:2d} | PID {c['pid']:>5s} | {c['cpu']:>4s}% CPU | {c['etime']:>5s}]"
                    mod_badge = f"{BLUE}[{c['module']}]{RESET}"
                    out.append(f"{prefix} {mod_badge} {c['source']}")
                if len(active_clangs) < clang_count:
                    diff = clang_count - len(active_clangs)
                    out.append(f"  {DIM}+ {diff} additional compiler worker(s) active on background threads...{RESET}")
            elif is_building:
                out.append(f"  {YELLOW}▶{RESET} {clang_count} active compiler processes dispatched across CPU cores")
            else:
                out.append(f"  {DIM}(No compiler processes currently executing){RESET}")

            out.append(f"{DIM}├{line_sep}┤{RESET}")

            # Recently Completed Targets Panel (strictly driven by live stdout step stream)
            out.append(f" {BOLD}Recently Completed Targets (Live Stream):{RESET}")
            if recent_completed:
                for item in list(recent_completed)[-5:]:
                    dur = item.get("duration_s")
                    if dur is None:
                        # dynamically look up in duration_map in case .ninja_log was just written
                        dur = duration_map.get(item["name"], duration_map.get(item.get("raw", ""), None))
                    dur_str = format_target_duration(dur)
                    if dur is not None and dur > 5.0:
                        sec_colored = f"{YELLOW}{dur_str:>6s}{RESET}"
                    else:
                        sec_colored = f"{DIM}{dur_str:>6s}{RESET}"
                    timing_badge = f"[{sec_colored}]"
                    out.append(f"  {GREEN}✔{RESET} {timing_badge} {CYAN}[{item['module']}]{RESET} {item['name']}")
            else:
                out.append(f"  {DIM}(Waiting for next target to complete...){RESET}")

            # Compiler Warnings & Errors Buffer (lightweight string checks only)
            if warning_count > 0 or error_count > 0 or recent_errors:
                out.append(f"{DIM}├{line_sep}┤{RESET}")
                err_badge = f"{RED}{BOLD}{error_count} Errors{RESET}" if error_count > 0 else f"{GREEN}0 Errors{RESET}"
                warn_badge = f"{YELLOW}{warning_count} Warnings{RESET}"
                out.append(f" {BOLD}Diagnostics:{RESET} {err_badge} │ {warn_badge}")
                
                if recent_errors:
                    for e in recent_errors:
                        out.append(f"  {RED}✖ {e[:width-6]}{RESET}")
                elif recent_warnings:
                    for w in list(recent_warnings)[-2:]:
                        out.append(f"  {YELLOW}⚠ {DIM}{w[:width-6]}{RESET}")

            out.append(f"{CYAN}{BOLD}╚{double_sep}╝{RESET}")
            out.append(f"{DIM}Press Ctrl+C to exit monitor. Auto-resumes on new builds.{RESET}")

            sys.stdout.write("\n".join(out) + "\n")
            sys.stdout.flush()

            time.sleep(0.35)

    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\033[?25h\n")
        sys.stdout.flush()

if __name__ == "__main__":
    main()
