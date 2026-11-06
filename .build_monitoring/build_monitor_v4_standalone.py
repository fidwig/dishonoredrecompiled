#!/usr/bin/env python3
"""
Dishonored macOS Native Build Monitor v4 (Standalone Edition)
Pure standalone monitor for manual Ninja/CMake builds - Zero AGY/task dependencies.
Run in any terminal window:
    python3 .build_monitoring/build_monitor_v4_standalone.py
Or from workspace root:
    ./build_monitor_v4_standalone.py
"""

import os
import re
import sys
import time
import glob
import signal
import argparse
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
ANSI_ESCAPE_RE = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")

KNOWN_TARGET_TOTALS = {
    "DishonoredGame": 658,
    "dishonoredgame": 658,
    "d9mt": 124,
    "D3D9Drv": 18,
    "d3d9drv": 18,
    "Engine": 503,
    "engine": 503,
    "Core": 70,
    "core": 70,
    "GameFramework": 24,
    "gameframework": 24,
    "IpDrv": 30,
    "ipdrv": 30,
}

def strip_ansi(s):
    """Remove ANSI escape sequences to compute exact visual column widths."""
    return ANSI_ESCAPE_RE.sub("", s)

def visible_len(s):
    """Return the visual character length of a formatted ANSI string."""
    return len(strip_ansi(s))

def get_terminal_width(default=92):
    try:
        return os.get_terminal_size().columns
    except Exception:
        return default

def format_duration(seconds):
    """Format duration into friendly MM:SS or HH:MM:SS."""
    if seconds is None or seconds < 0:
        return "--:--"
    m, s = divmod(int(round(seconds)), 60)
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

def count_compiler_processes():
    """Counts active clang, clang++, metal, metallib, air-lld compiler processes reliably via pgrep."""
    try:
        res = subprocess.run(
            ["pgrep", "-f", r"(clang\+\+|clang|metal|metallib|air-lld) "],
            capture_output=True,
            text=True,
            timeout=0.3
        )
        pids = [p for p in res.stdout.strip().split("\n") if p]
        return len(pids)
    except Exception:
        return 0

def classify_module(path_or_target):
    """Classifies a source file or build target into a clean subsystem category."""
    if not path_or_target:
        return "Engine"
    p = str(path_or_target)
    if "d9mt" in p or "mtl_" in p or "/air" in p or p.endswith((".metal", ".air", ".metallib")):
        return "d9mt"
    if "D3D9Drv" in p or "D3D9" in p:
        return "D3D9Drv"
    if "Core" in p:
        return "Core"
    if "GameFramework" in p:
        return "GameFramework"
    if "IpDrv" in p:
        return "IpDrv"
    if "GFx" in p or "gfx" in p:
        return "GFx3"
    if "Launch" in p or "DishonoredGame" in p or "Dishonored" in p:
        return "DishonoredGame"
    if "zlib" in p or "libpng" in p or "png" in p or "lzokay" in p or "_deps" in p:
        return "ThirdParty"
    return "Engine"

def get_active_compiler_processes():
    """Inspect active compiler command lines (clang++, clang, metal, metallib) via ps -ww -eo pid,%cpu,etime,command."""
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
            
            is_clang = ("clang++" in command_str or "clang" in command_str) and " -c " in command_str
            is_metal = "metal" in command_str and ("-c" in command_str or ".metal" in command_str or ".air" in command_str)
            is_metallib = "metallib" in command_str
            
            if is_clang or is_metal or is_metallib:
                source_file = "compiling..."
                tokens = command_str.split()
                if "-c" in tokens:
                    idx = tokens.index("-c")
                    if idx + 1 < len(tokens):
                        source_file = tokens[idx + 1]
                elif "-o" in tokens:
                    idx = tokens.index("-o")
                    if idx + 1 < len(tokens):
                        source_file = tokens[idx + 1]
                else:
                    for t in reversed(tokens):
                        if t.endswith((".cpp", ".c", ".cc", ".m", ".mm", ".metal", ".air")) and not t.startswith("-"):
                            source_file = t
                            break
                
                module = classify_module(source_file)
                if is_metal and module == "d9mt":
                    module = "Metal/AIR"
                elif is_metallib:
                    module = "Metallib"
                
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

def get_active_ninja_command():
    """Inspects the running ninja command line to identify the active build target and working dir."""
    target = None
    build_dir = None
    try:
        cmd = ["ps", "-ww", "-eo", "pid,command"]
        res = subprocess.run(cmd, capture_output=True, text=True, errors="ignore", timeout=0.5)
        if res.returncode == 0:
            for line in res.stdout.strip().splitlines():
                if "ninja" in line and not "python" in line and not "monitor" in line:
                    tokens = line.strip().split()
                    for i, tok in enumerate(tokens):
                        if tok == "-C" and i + 1 < len(tokens):
                            build_dir = tokens[i + 1]
                    for tok in reversed(tokens):
                        if not tok.startswith("-") and tok not in ["ninja", "ninja-build"]:
                            target = tok
                            break
                    break
    except Exception:
        pass
    return target, build_dir

def find_standalone_build_dir(hint=None):
    """Locates the active build directory containing build.ninja and .ninja_log."""
    if hint and os.path.isdir(hint):
        return os.path.abspath(hint)
    
    # Check if hint is a file (e.g. .ninja_log or build.ninja)
    if hint and os.path.isfile(hint):
        return os.path.abspath(os.path.dirname(hint))

    # Auto-detect from active ninja process
    _, active_dir = get_active_ninja_command()
    if active_dir and os.path.isdir(active_dir):
        return os.path.abspath(active_dir)

    candidates = [
        "build/darwin-debug",
        "build",
        "build/debug",
        "build/release",
        "out/darwin-debug",
        "out",
        ".",
    ]
    for c in candidates:
        if os.path.exists(os.path.join(c, "build.ninja")) or os.path.exists(os.path.join(c, ".ninja_log")):
            return os.path.abspath(c)
    return None

def find_manual_log_file(build_dir, hint_file=None):
    """Finds any user-piped stdout log (e.g., build.log, ninja.log) if present."""
    if hint_file and os.path.isfile(hint_file) and not hint_file.endswith(".ninja_log"):
        return os.path.abspath(hint_file)
    
    candidates = [
        "build.log",
        "ninja.log",
        "compile.log",
        "cmake.log",
    ]
    if build_dir:
        candidates.extend([
            os.path.join(build_dir, "build.log"),
            os.path.join(build_dir, "ninja.log"),
        ])
    for c in candidates:
        if os.path.exists(c) and os.path.getsize(c) > 0:
            return os.path.abspath(c)
    return None

def parse_ninja_log_durations(log_path):
    """Parse .ninja_log for target duration lookup map."""
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
    parser = argparse.ArgumentParser(description="Dishonored macOS Native Build Monitor v4 (Standalone)")
    parser.add_argument("path", nargs="?", default=None, help="Path to build directory or build log file")
    parser.add_argument("--dir", default=None, help="Explicit path to CMake/Ninja build directory")
    parser.add_argument("--log", default=None, help="Explicit path to user-piped stdout log file")
    args = parser.parse_args()

    def cleanup_and_exit(signum, frame):
        sys.stdout.write("\033[?25h\n")
        sys.stdout.flush()
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup_and_exit)
    signal.signal(signal.SIGTERM, cleanup_and_exit)

    # Hide cursor
    sys.stdout.write("\033[?25l")
    sys.stdout.flush()

    build_dir = find_standalone_build_dir(args.dir or args.path)
    manual_log = find_manual_log_file(build_dir, args.log or args.path)

    ninja_log_path = os.path.join(build_dir, ".ninja_log") if build_dir else None

    last_ninja_mtime = 0
    last_ninja_log_pos = 0
    duration_map = {}

    current_step = 0
    total_steps = 0
    current_action = "Standby"
    build_start_time = None
    is_build_completed = False

    recent_completed = deque(maxlen=7)
    current_module_counts = {}

    # Diagnostics
    warning_count = 0
    error_count = 0
    recent_errors = deque(maxlen=3)
    recent_warnings = deque(maxlen=3)

    # Calibrated Rate & Stable ETA state
    throughput_samples = deque(maxlen=180)
    smoothed_rate = None
    displayed_eta = None
    last_eta_update_time = None
    last_step_seen = 0

    spinner_idx = 0
    spinners = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

    # Initialize duration map
    if ninja_log_path and os.path.exists(ninja_log_path):
        duration_map = parse_ninja_log_durations(ninja_log_path)
        last_ninja_log_pos = os.path.getsize(ninja_log_path)

    try:
        while True:
            now = time.time()
            spinner_idx = (spinner_idx + 1) % len(spinners)
            spinner = spinners[spinner_idx]

            # Re-verify build directory if not discovered initially
            if not build_dir or not os.path.exists(build_dir):
                build_dir = find_standalone_build_dir(args.dir or args.path)
                if build_dir:
                    ninja_log_path = os.path.join(build_dir, ".ninja_log")

            # Check active compiler and Ninja processes
            pgrep_compiler_count = count_compiler_processes()
            active_compilers = get_active_compiler_processes()
            compiler_count = max(pgrep_compiler_count, len(active_compilers))
            ninja_running = is_process_running("ninja")

            # Detect active target from ninja command line
            active_target, _ = get_active_ninja_command()
            if active_target and total_steps == 0:
                base_target = os.path.basename(active_target)
                total_steps = KNOWN_TARGET_TOTALS.get(base_target, 0)

            # Check if .ninja_log has been modified
            ninja_log_active = False
            if ninja_log_path and os.path.exists(ninja_log_path):
                try:
                    st = os.stat(ninja_log_path)
                    ninja_mtime = st.st_mtime
                    if (now - ninja_mtime) < 4.0:
                        ninja_log_active = True
                except Exception:
                    pass

            # Active build condition
            is_building = (ninja_running or compiler_count > 0 or ninja_log_active)

            if is_building and build_start_time is None:
                build_start_time = now
                is_build_completed = False
            elif not is_building and current_step > 0 and (total_steps > 0 and current_step >= total_steps - 5):
                is_build_completed = True

            # Reset on brand new build detection
            if is_building and is_build_completed:
                is_build_completed = False
                current_step = 0
                build_start_time = now
                recent_completed.clear()
                current_module_counts.clear()
                throughput_samples.clear()
                smoothed_rate = None
                displayed_eta = None

            # Read new entries from .ninja_log
            if ninja_log_path and os.path.exists(ninja_log_path):
                try:
                    curr_size = os.path.getsize(ninja_log_path)
                    if curr_size < last_ninja_log_pos:
                        # Log truncated/re-written: reset pos
                        last_ninja_log_pos = 0
                        current_step = 0
                    
                    if curr_size > last_ninja_log_pos:
                        with open(ninja_log_path, "r", errors="ignore") as f:
                            f.seek(last_ninja_log_pos)
                            for line in f:
                                line = line.strip()
                                if not line or line.startswith("#"):
                                    continue
                                parts = line.split("\t")
                                if len(parts) >= 4:
                                    start_ms = int(parts[0])
                                    end_ms = int(parts[1])
                                    target_raw = parts[3]
                                    target_name = os.path.basename(target_raw)
                                    dur_s = max(0.01, (end_ms - start_ms) / 1000.0)

                                    duration_map[target_name] = dur_s
                                    duration_map[target_raw] = dur_s

                                    current_step += 1
                                    mod = classify_module(target_raw)
                                    current_module_counts[mod] = current_module_counts.get(mod, 0) + 1
                                    current_action = f"Building {target_name}"

                                    recent_completed.append({
                                        "name": target_name,
                                        "raw": target_raw,
                                        "module": mod,
                                        "duration_s": dur_s
                                    })
                            last_ninja_log_pos = f.tell()
                except Exception:
                    pass

            # Read optional manual log file if present for exact [x/y] steps and errors
            if manual_log and os.path.exists(manual_log):
                try:
                    with open(manual_log, "r", errors="ignore") as f:
                        f.seek(max(0, os.path.getsize(manual_log) - 64 * 1024))
                        for line in f:
                            if " warning:" in line:
                                warning_count += 1
                            elif " error:" in line or line.startswith("FAILED:"):
                                error_count += 1
                            m = NINJA_STEP_RE.search(line)
                            if m:
                                current_step = int(m.group(1))
                                total_steps = int(m.group(2))
                                current_action = m.group(3).strip()
                except Exception:
                    pass

            # If total_steps is unknown, adapt to at least current_step
            if total_steps > 0:
                if current_step > total_steps:
                    total_steps = current_step
            elif current_step > 0:
                total_steps = max(current_step, KNOWN_TARGET_TOTALS.get(active_target or "DishonoredGame", 658))

            # Throughput & ETA Calculation
            throughput_samples.append((now, current_step))
            elapsed_time = max(0.1, (now - build_start_time)) if build_start_time else 0.1

            cumulative_rate = (current_step / elapsed_time) if elapsed_time > 2.0 and current_step > 0 else 0.0

            instant_rate = 0.0
            if len(throughput_samples) >= 2:
                oldest_t, oldest_step = throughput_samples[0]
                dt = now - oldest_t
                dstep = current_step - oldest_step
                if dt > 1.5 and dstep >= 0:
                    instant_rate = dstep / dt

            if instant_rate > 0:
                if smoothed_rate is None:
                    smoothed_rate = instant_rate
                else:
                    smoothed_rate = 0.08 * instant_rate + 0.92 * smoothed_rate

            if smoothed_rate is not None and smoothed_rate > 0.01 and cumulative_rate > 0.01:
                blended_rate = 0.5 * cumulative_rate + 0.5 * smoothed_rate
            elif smoothed_rate is not None and smoothed_rate > 0.01:
                blended_rate = smoothed_rate
            elif cumulative_rate > 0.01:
                blended_rate = cumulative_rate
            else:
                blended_rate = 0.0

            remaining_steps = max(0, total_steps - current_step)

            # Historical estimate via duration map
            historical_eta = None
            if duration_map and remaining_steps > 0:
                valid_durations = [d for d in duration_map.values() if 0.05 <= d <= 120.0]
                if valid_durations:
                    avg_cpu_s = sum(valid_durations) / len(valid_durations)
                    eff_workers = max(1, min(12, compiler_count if compiler_count > 0 else 8))
                    historical_eta = (remaining_steps * avg_cpu_s) / eff_workers

            raw_eta = None
            if is_build_completed:
                raw_eta = 0.0
            elif remaining_steps > 0:
                if blended_rate > 0.02:
                    raw_eta = remaining_steps / blended_rate
                    if current_step < 25 and historical_eta is not None:
                        warmup_w = max(0.0, 1.0 - (current_step / 25.0)) * 0.35
                        raw_eta = (1.0 - warmup_w) * raw_eta + warmup_w * historical_eta
                elif historical_eta is not None:
                    raw_eta = historical_eta

            # Link Phase Floors
            act_upper = current_action.upper()
            is_linking_exe = ("LINKING CXX EXECUTABLE" in act_upper or "DishonoredGame" in current_action)
            is_linking_lib = ("LINKING CXX STATIC LIBRARY" in act_upper or "libEngine.a" in current_action or "libd9mt.a" in current_action)

            if not is_build_completed and is_building:
                if remaining_steps <= 1 and is_linking_exe:
                    raw_eta = max(raw_eta or 0.0, 10.0)
                elif remaining_steps <= 2 and is_linking_lib:
                    raw_eta = max(raw_eta or 0.0, 4.0)
                elif remaining_steps <= 2 and raw_eta is not None:
                    raw_eta = max(raw_eta, 3.0)

            # Monotonic Clamping
            if is_build_completed:
                displayed_eta = 0.0
            elif raw_eta is not None:
                if displayed_eta is None:
                    displayed_eta = raw_eta
                    last_eta_update_time = now
                    last_step_seen = current_step
                else:
                    time_delta = max(0.05, min(1.0, (now - last_eta_update_time) if last_eta_update_time else 0.35))
                    max_rise = 1.8 * time_delta
                    max_drop = 2.5 * time_delta
                    clamped_target = min(displayed_eta + max_rise, max(displayed_eta - max_drop, raw_eta))
                    displayed_eta = 0.82 * displayed_eta + 0.18 * clamped_target

                    if current_step == last_step_seen and displayed_eta > 2.0:
                        displayed_eta = max(1.5, displayed_eta - 0.75 * time_delta)

                    last_eta_update_time = now
                    last_step_seen = current_step

            # Phase Determination
            phase = "STANDBY"
            if is_build_completed:
                phase = "COMPLETED"
            elif is_building:
                if ("LINKING CXX EXECUTABLE" in act_upper or "DishonoredGame" in current_action) and ("LINK" in act_upper or "DishonoredGame" in current_action):
                    phase = "LINKING EXECUTABLE (DishonoredGame)"
                elif "LINKING CXX STATIC LIBRARY" in act_upper and ("D9MT" in act_upper or "D3D9" in act_upper):
                    phase = "LINKING METAL RHI (libd9mt.a / libD3D9Drv.a)"
                elif "LINKING CXX STATIC LIBRARY" in act_upper:
                    lib_name = current_action.split()[-1] if current_action else "archive"
                    phase = f"ARCHIVING {os.path.basename(lib_name)}"
                elif "D9MT" in act_upper or "MTL_" in act_upper or "/AIR" in current_action.lower():
                    phase = "COMPILING D9MT METAL TRANSLATION LAYER"
                elif "D3D9DRV" in act_upper or "D3D9" in act_upper:
                    phase = "COMPILING D3D9DRV (METAL BACKEND)"
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
                elif "metal" in current_action.lower() or "metallib" in current_action.lower():
                    phase = "COMPILING METAL SHADERS (AIR)"
                else:
                    phase = "COMPILING C++ UNITS"
            else:
                phase = "IDLE / WAITING"

            # Render UI
            width = max(84, min(120, get_terminal_width()))
            line_sep = "─" * (width - 2)
            double_sep = "═" * (width - 2)

            out = []
            out.append(CLEAR_SCREEN)

            # Precise Title Bar Alignment
            title_content = f" {BOLD}Dishonored macOS Native Build Monitor v4{RESET} {DIM}(Standalone Edition - Apple Silicon){RESET} "
            vis_title_len = visible_len(title_content)
            pad_len = max(0, (width - 2) - vis_title_len)
            title_line = f"{CYAN}{BOLD}║{RESET}{title_content}{' ' * pad_len}{CYAN}{BOLD}║{RESET}"
            out.append(f"{CYAN}{BOLD}╔{double_sep}╗{RESET}")
            out.append(title_line)
            out.append(f"{CYAN}{BOLD}╚{double_sep}╝{RESET}")

            # Status Header
            if is_build_completed:
                status_str = f"{GREEN}{BOLD}✔ BUILD COMPLETE & LINKED{RESET}"
            elif is_building:
                status_str = f"{YELLOW}{BOLD}{spinner} BUILDING{RESET} ({compiler_count} active compiler threads)"
            else:
                status_str = f"{DIM}💤 IDLE (Waiting for manual ninja command...){RESET}"

            out.append(f" {BOLD}Status:{RESET}  {status_str}")
            out.append(f" {BOLD}Phase:{RESET}   {MAGENTA}{BOLD}{phase}{RESET}")

            # Performance & System Metrics
            load_str, ram_str = get_system_metrics()
            if is_build_completed:
                rate_str = "done"
                eta_str = "00:00"
            else:
                rate_str = f"{blended_rate:4.1f} files/sec" if blended_rate > 0 else "evaluating..."
                eta_str = format_duration(displayed_eta) if displayed_eta is not None else "calculating..."
            
            elapsed_str = format_duration(elapsed_time) if build_start_time else "--:--"

            build_dir_label = os.path.basename(build_dir) if build_dir else "auto"
            out.append(f" {BOLD}Metrics:{RESET} Rate: {GREEN}{rate_str}{RESET} │ ETA: {CYAN}{eta_str}{RESET} │ Elapsed: {elapsed_str} │ Load: {DIM}{load_str}{RESET} │ Dir: {DIM}{build_dir_label}{RESET}")
            out.append(f"{DIM}┌{line_sep}┐{RESET}")

            # Progress Bar Section
            if total_steps > 0:
                display_step = total_steps if is_build_completed else min(current_step, total_steps)
                bar = make_progress_bar(display_step, total_steps, width=int(width * 0.42))
                out.append(f"  Progress: {GREEN}{BOLD}{bar}{RESET}  ({display_step} / {total_steps} targets)")
            else:
                out.append(f"  Progress: {DIM}[░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] Standby (Awaiting Ninja){RESET}")

            # Module Breakdown Pills
            if current_module_counts:
                pills = []
                for mod, cnt in sorted(current_module_counts.items(), key=lambda x: -x[1]):
                    pills.append(f"{CYAN}{mod}:{RESET}{BOLD}{cnt}{RESET}")
                out.append(f"  Modules:  {' │ '.join(pills[:7])}")

            out.append(f"{DIM}├{line_sep}┤{RESET}")

            # Multi-Core Live Compiler Tracking Panel
            out.append(f" {BOLD}Live Compiler Tracking across CPU Cores ({compiler_count} active workers):{RESET}")
            if active_compilers:
                for i, c in enumerate(active_compilers[:10]):
                    prefix = f"  {YELLOW}▶{RESET} [Core {i+1:2d} | PID {c['pid']:>5s} | {c['cpu']:>4s}% CPU | {c['etime']:>5s}]"
                    mod_badge = f"{BLUE}[{c['module']}]{RESET}"
                    out.append(f"{prefix} {mod_badge} {c['source']}")
                if len(active_compilers) < compiler_count:
                    diff = compiler_count - len(active_compilers)
                    out.append(f"  {DIM}+ {diff} additional compiler worker(s) active on background threads...{RESET}")
            elif is_building:
                out.append(f"  {YELLOW}▶{RESET} {compiler_count} active compiler processes dispatched across CPU cores")
            else:
                out.append(f"  {DIM}(No compiler processes currently executing){RESET}")

            out.append(f"{DIM}├{line_sep}┤{RESET}")

            # Recently Completed Targets Panel
            out.append(f" {BOLD}Recently Completed Targets (.ninja_log):{RESET}")
            if recent_completed:
                for item in list(recent_completed)[-5:]:
                    dur = item.get("duration_s")
                    dur_str = format_target_duration(dur)
                    if dur is not None and dur > 5.0:
                        sec_colored = f"{YELLOW}{dur_str:>6s}{RESET}"
                    else:
                        sec_colored = f"{DIM}{dur_str:>6s}{RESET}"
                    timing_badge = f"[{sec_colored}]"
                    out.append(f"  {GREEN}✔{RESET} {timing_badge} {CYAN}[{item['module']}]{RESET} {item['name']}")
            else:
                out.append(f"  {DIM}(Waiting for next build step...){RESET}")

            out.append(f"{CYAN}{BOLD}╚{double_sep}╝{RESET}")
            out.append(f"{DIM}Standalone monitor active. Auto-resumes whenever 'ninja' runs.{RESET}")

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
