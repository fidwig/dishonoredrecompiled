# /btw can you give me a script i can play in another terminal window so i can monitor the current build progress with like a progress bar
#  and maybe the last few compiled file names and what step is currently being done, and it would have to show any active task like when
#  this build is done, it will show it as done but once another build starts it shows this one without me having to restart the script?
# 
# Called 15 tools (14 run_command, 1 manage_task: 1 failed)
# 
# Here is a standalone, zero-dependency Python script you can run in another terminal window. It automatically detects running builds,
# tracks progress with an animated progress bar, shows the current file being compiled along with recent history, and automatically
# transitions between BUILDING, FINISHED, and IDLE states—staying alive to automatically catch any new build that starts without
# restarting the script.

### monitor_build.py

#!/usr/bin/env python3
"""
Antigravity & Ninja Real-Time Build Monitor
Run in any terminal window:
	python3 monitor_build.py
"""

import os
import re
import subprocess
import sys
import time
import glob

# ANSI Color codes
BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
RED = "\033[31m"
BLUE = "\033[34m"
CLEAR_SCREEN = "\033[H\033[J"

NINJA_STEP_RE = re.compile(r"\[(\d+)/(\d+)\]\s+(.*)")

def is_process_running(proc_name):
	"""Check if ninja or clang is actively running."""
	try:
		res = subprocess.run(["pgrep", "-x", proc_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
		return res.returncode == 0
	except Exception:
		return False

# def count_clang_processes():
# 	"""Count how many clang++ compiler jobs are actively executing."""
# 	try:
# 		res = subprocess.run(["pgrep", "-c", "clang++"], capture_output=True, text=True)
# 		return int(res.stdout.strip()) if res.stdout.strip().isdigit() else 0
# 	except Exception:
# 		return 0

def count_clang_processes():
	"""Counts active clang / clang++ compiler processes reliably on macOS."""
	try:
		# Use pgrep matching command lines for clang/clang++
		res = subprocess.run(
			["pgrep", "-f", r"(clang\+\+|clang) "],
			capture_output=True,
			text=True
		)
		pids = [p for p in res.stdout.strip().split("\n") if p]
		return len(pids)
	except Exception:
		return 0

def find_active_log_file():
	"""Find the most relevant/recent task log from ~/.gemini/antigravity-cli or local dir."""
	candidates = []

	# 1. Antigravity task logs across all conversation brains
	brain_glob = os.path.expanduser("~/.gemini/antigravity-cli/brain/*/.system_generated/tasks/task-*.log")
	candidates.extend(glob.glob(brain_glob))

	# 2. Local workspace logs if any
	candidates.extend(glob.glob("build/**/task-*.log", recursive=True))

	if not candidates:
		return None

	# Sort by modification time, newest first
	candidates.sort(key=lambda p: os.path.getmtime(p), reverse=True)
	return candidates[0]

def make_progress_bar(current, total, width=38):
	if total <= 0:
		return "[" + " " * width + "] 0.0%"
	pct = min(1.0, max(0.0, current / total))
	filled = int(round(width * pct))
	bar = "█" * filled + "░" * (width - filled)
	return f"[{bar}] {pct * 100:5.1f}%"

def main():
	last_log_path = None
	last_mtime = 0
	current_step = 0
	total_steps = 0
	current_action = "Idle"
	recent_history = []
	build_active = False
	spinner_idx = 0
	spinners = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

	# Hide cursor
	sys.stdout.write("\033[?25l")
	sys.stdout.flush()

	try:
		while True:
			spinner_idx = (spinner_idx + 1) % len(spinners)
			spinner = spinners[spinner_idx]

			# Check system processes
			ninja_running = is_process_running("ninja")
			clang_count = count_clang_processes()

			# Find newest log
			log_path = find_active_log_file()

			# If log switched (new build started)
			if log_path != last_log_path:
				last_log_path = log_path
				last_mtime = 0
				current_step = 0
				total_steps = 0
				recent_history = []

			# Read new lines from log if updated
			if log_path and os.path.exists(log_path):
				mtime = os.path.getmtime(log_path)
				if mtime != last_mtime:
					last_mtime = mtime
					try:
						with open(log_path, "r", errors="ignore") as f:
							lines = f.readlines()
							for line in lines[-200:]:  # Scan recent lines
								match = NINJA_STEP_RE.search(line)
								if match:
									step = int(match.group(1))
									total = int(match.group(2))
									action = match.group(3).strip()

									# Reset if new build started in same file
									if step < current_step:
										recent_history = []

									current_step = step
									total_steps = total
									current_action = action

									# Track completed targets in history
									target_name = action.split()[-1] if action else ""
									if target_name and target_name not in recent_history:
										recent_history.append(target_name)
										if len(recent_history) > 6:
											recent_history.pop(0)
					except Exception:
						pass

			# Determine State
			if ninja_running or clang_count > 0:
				status_header = f"{YELLOW}{BOLD}{spinner} BUILDING{RESET} ({clang_count} active clang threads)"
				build_active = True
			elif current_step > 0 and current_step >= total_steps:
				status_header = f"{GREEN}{BOLD}✓ BUILD COMPLETE{RESET}"
				build_active = False
			elif build_active and not ninja_running:
				status_header = f"{GREEN}{BOLD}✓ FINISHED / IDLE{RESET}"
				build_active = False
			else:
				status_header = f"{DIM}💤 IDLE (Waiting for next build task...){RESET}"

			# Format terminal output
			out = []
			out.append(CLEAR_SCREEN)
			out.append(f"{CYAN}{BOLD}================================================================={RESET}")
			out.append(f" {BOLD}Dishonored macOS Native Build Monitor{RESET}")
			out.append(f"{CYAN}{BOLD}================================================================={RESET}")
			out.append(f" Status:   {status_header}")

			if last_log_path:
				log_name = os.path.basename(last_log_path)
				out.append(f" Task Log: {DIM}{log_name}{RESET}")

			out.append("")

			# Progress Bar Section
			if total_steps > 0:
				bar = make_progress_bar(current_step, total_steps)
				out.append(f" Progress: {GREEN}{BOLD}{bar}{RESET}  ({current_step} / {total_steps} tasks)")
			else:
				out.append(f" Progress: {DIM}[░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] Standby{RESET}")

			out.append("")

			# Current action
			short_action = (current_action[:72] + "...") if len(current_action) > 75 else current_action
			out.append(f" {BOLD}Current Step:{RESET}")
			out.append(f"   {CYAN}▸ {short_action}{RESET}")
			out.append("")

			# Recent History
			out.append(f" {BOLD}Recently Completed Files:{RESET}")
			if recent_history:
				for item in reversed(recent_history):
					basename = os.path.basename(item)
					out.append(f"   {GREEN}✔{RESET} {basename} {DIM}({item}){RESET}")
			else:
				out.append(f"   {DIM}(None yet){RESET}")

			out.append("")
			out.append(f"{DIM}Press Ctrl+C to exit monitor. Auto-resumes on new builds.{RESET}")
			out.append(f"{CYAN}{BOLD}================================================================={RESET}")

			sys.stdout.write("\n".join(out) + "\n")
			sys.stdout.flush()

			time.sleep(0.4)

	except KeyboardInterrupt:
		pass
	finally:
		# Restore cursor
		sys.stdout.write("\033[?25h\n")
		sys.stdout.flush()

if __name__ == "__main__":
	main()



# ### How to Run It:

# Open a new terminal tab/window and run:

# 	# Save and run directly from anywhere:
# 	curl -s -o ~/monitor_build.py https://... 2>/dev/null || cat << 'EOF' > ~/monitor_build.py
# 	# (Paste the script above into ~/monitor_build.py)
# 	EOF

# 	python3 ~/monitor_build.py

# Or as a quick one-liner:

# 	python3 -c "$(pbpaste)"

# (Copy the script block above, then paste and run the one-liner in your terminal).
