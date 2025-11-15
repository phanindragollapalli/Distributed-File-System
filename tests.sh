#!/usr/bin/env bash
# grade_course_project.sh
# Single comprehensive test + grading harness for Docs++ project layout.
# Place at project root (sibling to docs-plus-plus/). Make executable and run.
#
# Behavior:
# - Best-effort start of Name Server and Storage Server (from bin/ or scripts/)
# - Uses client binary/script to send commands (or nc fallback)
# - Runs functional tests for all user commands, system requirements, and some bonus checks
# - Tallies points (TOTAL_POSSIBLE) and prints failed testcases at the end
#
# NOTE: This harness is tolerant of different CLI outputs by searching for keywords.
#       If your program uses different flags or formats, edit the send_client() wrapper below.

set -o pipefail
export PATH="$PATH:."

# ---------- Configuration (edit if necessary) ----------
BASE="./course_project"

NM_BIN="$BASE/bin/name_server"
SS_BIN="$BASE/bin/storage_server"
CLIENT_BIN="$BASE/bin/client"

NM_START_SCRIPT="$BASE/scripts/start_ns.sh"
SS_START_SCRIPT="$BASE/scripts/start_ss.sh"
CLIENT_START_SCRIPT="$BASE/scripts/start_client.sh"

NM_LOG="$BASE/name_server/logs/ns.log"
SS_LOG="$BASE/storage_server/logs/ss.log"
SS_DATA_DIR="$BASE/storage_server/storage/files"

# Networking (best-effort defaults)
NM_HOST="127.0.0.1"
NM_PORT=9000
SS_HOST="127.0.0.1"
SS_PORT=9100

# Timeout for client operations (seconds)
TIMEOUT_CMD=6

# Users used in tests
USER1="alice"
USER2="bob"

# Scoring
TOTAL_POSSIBLE=250
score=0
failed=()

# Convenience
h1() { echo -e "\n===== $* ====="; }
h2() { echo -e "\n----- $* -----"; }
warn() { echo -e "WARN: $*" >&2; }
err() { echo -e "ERROR: $*" >&2; }

# Ensure cleanup
pids_to_kill=()
cleanup() {
  echo "Cleaning up..."
  for pid in "${pids_to_kill[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" || true
    fi
  done
}
trap cleanup EXIT

# ---------- Start services (best-effort) ----------
start_nm() {
  mkdir -p "$(dirname "$NM_LOG")"
  if [[ -x "$NM_BIN" ]]; then
    rm -f "$NM_LOG"
    echo "Starting Name Server binary: $NM_BIN (log -> $NM_LOG)"
    "$NM_BIN" --port "$NM_PORT" >"$NM_LOG" 2>&1 &
    nm_pid=$!
    pids_to_kill+=( "$nm_pid" )
    sleep 0.6
    echo "NM pid=$nm_pid"
    return 0
  elif [[ -x "$NM_START_SCRIPT" ]]; then
    echo "Starting Name Server script: $NM_START_SCRIPT"
    bash "$NM_START_SCRIPT" >"$NM_LOG" 2>&1 &
    nm_pid=$!
    pids_to_kill+=( "$nm_pid" )
    sleep 0.6
    return 0
  else
    warn "No NM binary/script found. Assuming NM already running or tests may fail."
    return 1
  fi
}

start_ss() {
  mkdir -p "$SS_DATA_DIR" "$(dirname "$SS_LOG")"
  if [[ -x "$SS_BIN" ]]; then
    rm -f "$SS_LOG"
    echo "Starting Storage Server binary: $SS_BIN (log -> $SS_LOG)"
    "$SS_BIN" --nm "$NM_HOST:$NM_PORT" --port "$SS_PORT" --data "$SS_DATA_DIR" >"$SS_LOG" 2>&1 &
    ss_pid=$!
    pids_to_kill+=( "$ss_pid" )
    sleep 0.6
    echo "SS pid=$ss_pid"
    return 0
  elif [[ -x "$SS_START_SCRIPT" ]]; then
    echo "Starting Storage Server script: $SS_START_SCRIPT"
    bash "$SS_START_SCRIPT" >"$SS_LOG" 2>&1 &
    ss_pid=$!
    pids_to_kill+=( "$ss_pid" )
    sleep 0.6
    return 0
  else
    warn "No SS binary/script found. Assuming SS already running or tests may fail."
    return 1
  fi
}

# ---------- Client invocation wrapper ----------
# send_client <user> <line1> <line2> ...
# Sends one or more lines to client, returns output.
send_client() {
  local user="$1"; shift
  local payload
  payload="$(printf "%s\n" "$@")"
  # If client binary exists and is executable, run it with nm host and user flags (common pattern).
  if [[ -x "$CLIENT_BIN" ]]; then
    printf "%s\n" "$payload" | timeout "$TIMEOUT_CMD" "$CLIENT_BIN" --nm "$NM_HOST:$NM_PORT" --user "$user"
    return $?
  fi
  # If client start script exists, use it
  if [[ -x "$CLIENT_START_SCRIPT" ]]; then
    printf "%s\n" "$payload" | timeout "$TIMEOUT_CMD" bash "$CLIENT_START_SCRIPT" --nm "$NM_HOST:$NM_PORT" --user "$user"
    return $?
  fi
  # Fallback: if NM speaks a simple TCP line protocol, use nc to send directly to NM (best-effort)
  if command -v nc >/dev/null 2>&1; then
    printf "%s\n" "$payload" | timeout "$TIMEOUT_CMD" nc "$NM_HOST" "$NM_PORT"
    return $?
  fi
  # No client available
  err "No client binary or script and nc not available. Cannot send client commands."
  return 2
}

# ---------- Assertion helpers ----------
assert_contains() {
  # args: expected, actual, description, points
  local expected="$1"; local actual="$2"; local desc="$3"; local pts="$4"
  if grep -Fq -- "$expected" <<<"$actual"; then
    echo "PASS: $desc (+$pts)"; score=$((score + pts)); return 0
  else
    echo "FAIL: $desc (0/$pts) — expected to find '$expected'"; failed+=( "$desc — missing '$expected'" ); return 1
  fi
}

assert_match_regex() {
  # args: regex, actual, description, points
  local regex="$1"; local actual="$2"; local desc="$3"; local pts="$4"
  if grep -Eiq -- "$regex" <<<"$actual"; then
    echo "PASS: $desc (+$pts)"; score=$((score + pts)); return 0
  else
    echo "FAIL: $desc (0/$pts) — expected regex '$regex'"; failed+=( "$desc — expected regex '$regex'" ); return 1
  fi
}

# ---------- Start services ----------
h1 "Starting services (best-effort)"
start_nm || true
start_ss || true
sleep 0.9

# ---------- RUN TEST SUITE ----------
h1 "BEGIN TEST SUITE"

# Helper: small wrapper to show command and capture output
run_cmd() {
  local u="$1"; shift
  echo -e "\n>> [client as ${u}] ${*//$'\n'/\\n}"
  timeout "$TIMEOUT_CMD" bash -c "send_client \"$u\" \"$@\"" 2>/dev/null
}

# 1) VIEW / CREATE (10)
h2 "1) VIEW & CREATE (10)"
out="$(send_client "$USER1" "CREATE test1.txt" 2>&1 || true)"
echo "CREATE -> ${out//$'\n'/\\n}"
out="$(send_client "$USER1" "VIEW" 2>&1 || true)"
assert_contains "test1.txt" "$out" "VIEW lists created file" 6
out="$(send_client "$USER1" "CREATE test1.txt" 2>&1 || true)"
if [[ "$out" =~ [Ee][Rr][Rr][Oo][Rr] || "$out" =~ "already" ]]; then
  echo "PASS: Duplicate CREATE returned error (+4)"; score=$((score + 4))
else
  echo "FAIL: Duplicate CREATE did not report error (0/4)"; failed+=( "Duplicate CREATE: expected error" )
fi

# 2) WRITE & READ (30)
h2 "2) WRITE & READ (30)"
# Write simple content using assumed protocol: WRITE filename sentence_index <lines> ETIRW
out="$(send_client "$USER1" "WRITE test1.txt 0" "1 Hello from test harness." "ETIRW" 2>&1 || true)"
echo "WRITE -> ${out//$'\n'/\\n}"
out="$(send_client "$USER1" "READ test1.txt" 2>&1 || true)"
assert_contains "Hello from test harness." "$out" "READ returns written content" 15

# invalid index write should produce an error
out="$(send_client "$USER1" "WRITE test1.txt 999" "1 should fail" "ETIRW" 2>&1 || true)"
if [[ "$out" =~ [Ee]rror || "$out" =~ "out of range" ]]; then
  echo "PASS: WRITE invalid index returned error (+15)"; score=$((score + 15))
else
  echo "FAIL: WRITE invalid index did not return expected error (0/15)"; failed+=( "WRITE invalid index: expected error" )
fi

# 3) UNDO (15)
h2 "3) UNDO (15)"
send_client "$USER1" "WRITE test1.txt 0" "1 pre-undo" "ETIRW" >/dev/null 2>&1 || true
send_client "$USER1" "WRITE test1.txt 0" "1 post-undo" "ETIRW" >/dev/null 2>&1 || true
out="$(send_client "$USER1" "UNDO test1.txt" 2>&1 || true)"
if [[ "$out" =~ [Uu]ndo || "$out" =~ [Ss]uccess ]]; then
  echo "PASS: UNDO reported success (+8)"; score=$((score + 8))
else
  echo "FAIL: UNDO did not report success (0/8)"; failed+=( "UNDO command: no success message" )
fi
out="$(send_client "$USER1" "READ test1.txt" 2>&1 || true)"
if grep -q "pre-undo" <<<"$out" && ! grep -q "post-undo" <<<"$out"; then
  echo "PASS: UNDO reverted to previous content (+7)"; score=$((score + 7))
else
  echo "FAIL: UNDO did not revert content properly (0/7)"; failed+=( "UNDO did not revert most recent change" )
fi

# 4) INFO & DELETE (10)
h2 "4) INFO & DELETE (10)"
out="$(send_client "$USER1" "INFO test1.txt" 2>&1 || true)"
assert_match_regex "Owner|owner" "$out" "INFO shows owner metadata" 4
assert_match_regex "Size|size" "$out" "INFO shows size metadata" 3
out="$(send_client "$USER1" "DELETE test1.txt" 2>&1 || true)"
if [[ "$out" =~ [Dd]eleted || "$out" =~ [Ss]uccess ]]; then
  echo "PASS: DELETE reports success (+3)"; score=$((score + 3))
else
  echo "FAIL: DELETE did not report success (0/3)"; failed+=( "DELETE: no success message" )
fi
out="$(send_client "$USER1" "VIEW -a" 2>&1 || true)"
if ! grep -q "test1.txt" <<<"$out"; then
  echo "PASS: Deleted file not present in listing (+0 extra)"; # already scored
else
  failed+=( "DELETE: file still listed after delete" )
fi

# 5) ACCESS CONTROL (15)
h2 "5) ACCESS CONTROL (15)"
send_client "$USER1" "CREATE access_test.txt" >/dev/null 2>&1 || true
out="$(send_client "$USER1" "ADDACCESS -R access_test.txt $USER2" 2>&1 || true)"
if [[ "$out" =~ [Gg]rant || "$out" =~ [Ss]uccess ]]; then
  echo "PASS: ADDACCESS reported success (+5)"; score=$((score + 5))
else
  warn "ADDACCESS message may differ; continuing"; failed+=( "ADDACCESS: no clear success message" )
fi
out="$(send_client "$USER2" "READ access_test.txt" 2>&1 || true)"
if [[ -z "$out" || "$out" =~ [Ee]rror ]]; then
  echo "FAIL: User2 could not read after ADDACCESS (0/5)"; failed+=( "ADDACCESS ineffective: user2 couldn't read" )
else
  echo "PASS: User2 read file after ADDACCESS (+5)"; score=$((score + 5))
fi
out="$(send_client "$USER1" "REMACCESS access_test.txt $USER2" 2>&1 || true)"
if [[ "$out" =~ [Rr]emov || "$out" =~ [Ss]uccess ]]; then
  echo "PASS: REMACCESS reported success (+5)"; score=$((score + 5))
else
  warn "REMACCESS message may differ"; failed+=( "REMACCESS: no clear success message" )
fi
# confirm removal
out="$(send_client "$USER2" "READ access_test.txt" 2>&1 || true)"
if [[ "$out" =~ [Ee]rror || -z "$out" ]]; then
  echo "PASS: user2 cannot read after REMACCESS (expected)"
else
  failed+=( "REMACCESS ineffective: user2 still can read" )
fi

# 6) STREAM (15)
h2 "6) STREAM (15)"
send_client "$USER1" "CREATE stream_test.txt" >/dev/null 2>&1 || true
send_client "$USER1" "WRITE stream_test.txt 0" "1 streaming unit test." "ETIRW" >/dev/null 2>&1 || true
t0=$(date +%s%3N)
out="$(send_client "$USER1" "STREAM stream_test.txt" 2>&1 || true)"
t1=$(date +%s%3N)
assert_contains "streaming" "$out" "STREAM outputs content" 10
elapsed_ms=$((t1 - t0))
if (( elapsed_ms >= 50 )); then
  echo "PASS: STREAM shows streaming behavior (+5)"; score=$((score + 5))
else
  warn "STREAM was fast (${elapsed_ms}ms) — timing points not awarded"; failed+=( "STREAM timing suspicious (${elapsed_ms}ms)" )
fi

# 7) EXEC (15)
h2 "7) EXEC (15)"
send_client "$USER1" "CREATE exec_test.txt" >/dev/null 2>&1 || true
send_client "$USER1" "WRITE exec_test.txt 0" "1 echo HELLO_EXEC" "ETIRW" >/dev/null 2>&1 || true
out="$(send_client "$USER1" "EXEC exec_test.txt" 2>&1 || true)"
if grep -q "HELLO_EXEC" <<<"$out"; then
  echo "PASS: EXEC executed content and returned output (+10)"; score=$((score + 10))
else
  echo "FAIL: EXEC did not return expected output (0/10)"; failed+=( "EXEC did not execute or return output" )
fi
# Check NM log for evidence of exec (best-effort)
if [[ -f "$NM_LOG" ]] && grep -qi "exec_test.txt\|EXEC" "$NM_LOG" 2>/dev/null; then
  echo "PASS: NM logged exec activity (+5)"; score=$((score + 5))
else
  warn "NM log lacks explicit exec evidence"; failed+=( "EXEC: NM log lacked execution entry" )
fi

# 8) UNAUTHORIZED WRITE rejection (5)
h2 "8) Unauthorized write rejection (5)"
send_client "$USER1" "CREATE ac_test2.txt" >/dev/null 2>&1 || true
out="$(send_client "$USER2" "WRITE ac_test2.txt 0" "1 hack attempt" "ETIRW" 2>&1 || true)"
if [[ "$out" =~ [Ee]rror || "$out" =~ [Ff]orbid || "$out" =~ [Uu]nauthorized ]]; then
  echo "PASS: Unauthorized write rejected (+5)"; score=$((score + 5))
else
  echo "FAIL: Unauthorized write not rejected (0/5)"; failed+=( "Access control: unauthorized write allowed or no error" )
fi

# 9) Persistence (10)
h2 "9) Persistence (10)"
send_client "$USER1" "CREATE persist_test.txt" >/dev/null 2>&1 || true
send_client "$USER1" "WRITE persist_test.txt 0" "1 persistent content." "ETIRW" >/dev/null 2>&1 || true
if [[ -n "${ss_pid-}" ]]; then
  echo "Restarting managed storage server to test persistence..."
  kill "$ss_pid" 2>/dev/null || true
  wait "$ss_pid" 2>/dev/null || true
  pids_to_kill=("${pids_to_kill[@]/$ss_pid}")
  start_ss || true
  sleep 0.8
fi
out="$(send_client "$USER1" "READ persist_test.txt" 2>&1 || true)"
if grep -q "persistent content." <<<"$out"; then
  echo "PASS: Data persisted after storage restart (+10)"; score=$((score + 10))
else
  echo "FAIL: Data not found after storage restart (0/10)"; failed+=( "Persistence: file not recovered after SS restart" )
fi

# 10) Logging presence (5)
h2 "10) Logging presence (5)"
if [[ -f "$NM_LOG" ]] && grep -Eiq "CREATE|WRITE|DELETE|EXEC|ADDACCESS|REMACCESS" "$NM_LOG" 2>/dev/null; then
  echo "PASS: NM logging captures operations (+3)"; score=$((score + 3))
else
  warn "NM log missing or not capturing expected operations"; failed+=( "Logging: NM log missing or incomplete" )
fi
if [[ -f "$SS_LOG" ]] && grep -Eiq "WRITE|READ|DELETE|STREAM" "$SS_LOG" 2>/dev/null; then
  echo "PASS: SS logging captures operations (+2)"; score=$((score + 2))
else
  warn "SS log missing or not capturing expected operations"; failed+=( "Logging: SS log missing or incomplete" )
fi

# 11) Search responsiveness / VIEW -a with many files (15)
h2 "11) Search responsiveness (15)"
echo "Creating many files for responsiveness test..."
for i in $(seq 1 150); do
  send_client "$USER1" "CREATE bigfile_${i}.txt" >/dev/null 2>&1 || true
done
t0=$(date +%s)
out="$(send_client "$USER1" "VIEW -a" 2>&1 || true)"
t1=$(date +%s)
dt=$((t1 - t0))
if (( dt <= 3 )); then
  echo "PASS: VIEW -a returned quickly (${dt}s) (+10)"; score=$((score + 10))
else
  echo "WARN: VIEW -a slow (${dt}s) (+5)"; score=$((score + 5)); failed+=( "VIEW -a slow (${dt}s)" )
fi
if [[ -f "$NM_LOG" ]] && grep -qi "cache\|search" "$NM_LOG" 2>/dev/null; then
  echo "PASS: NM log shows caching/search hints (+5)"; score=$((score + 5))
else
  warn "No cache/search hints found in NM log"; failed+=( "Efficient search: no cache/search evidence in NM log" )
fi

# ---------- Bonus best-effort checks (50 points total possible, optional) ----------
h2 "BONUS (optional) — best-effort checks (50 pts)"
bonus=0

# Bonus A: Folders / MOVE (10)
if send_client "$USER1" "CREATEFOLDER myfolder" >/dev/null 2>&1; then
  out="$(send_client "$USER1" "CREATEFOLDER myfolder" 2>&1 || true)"
  if [[ "$out" =~ [Cc]reated || "$out" =~ [Ss]uccess || "$out" =~ [Ee]xists ]]; then
    echo "PASS: CREATEFOLDER exists (+5)"; bonus=$((bonus + 5))
  fi
  if send_client "$USER1" "MOVE bigfile_1.txt myfolder" >/dev/null 2>&1; then
    echo "PASS: MOVE supported (+5)"; bonus=$((bonus + 5))
  fi
else
  echo "INFO: CREATEFOLDER not provided (skipping folder bonus)"
fi

# Bonus B: Checkpoints (15)
if send_client "$USER1" "CHECKPOINT bigfile_2.txt v1" >/dev/null 2>&1; then
  out="$(send_client "$USER1" "CHECKPOINT bigfile_2.txt v1" 2>&1 || true)"
  if [[ "$out" =~ [Cc]reated || "$out" =~ [Ss]uccess ]]; then
    echo "PASS: CHECKPOINT created (+5)"; bonus=$((bonus + 5))
  fi
  if send_client "$USER1" "VIEWCHECKPOINT bigfile_2.txt v1" >/dev/null 2>&1; then
    echo "PASS: VIEWCHECKPOINT supported (+5)"; bonus=$((bonus + 5))
  fi
  if send_client "$USER1" "REVERT bigfile_2.txt v1" >/dev/null 2>&1; then
    echo "PASS: REVERT supported (+5)"; bonus=$((bonus + 5))
  fi
else
  echo "INFO: Checkpoint commands not present (skipping checkpoint bonus)"
fi

# Bonus C: Replication/fault tolerance hints (15)
if [[ -f "$NM_LOG" ]] && grep -qi "replica\|replication\|replicate" "$NM_LOG" 2>/dev/null; then
  echo "PASS: replication mentions in NM log (+10)"; bonus=$((bonus + 10))
fi
if [[ -f "$NM_LOG" ]] && grep -qi "storage server\|storage_server\|SS" "$NM_LOG" 2>/dev/null; then
  echo "PASS: NM recorded storage servers (+5)"; bonus=$((bonus + 5))
fi

echo "Bonus subtotal: $bonus / 50"
score=$((score + bonus))

# ---------- Specification checks (10 pts) ----------
h2 "SPEC checks (10 pts)"
if [[ -n "${nm_pid-}" ]] && kill -0 "$nm_pid" 2>/dev/null; then
  echo "PASS: NM process running (+3)"; score=$((score + 3))
else
  warn "NM not started by harness (no points)."; failed+=( "Spec: NM not running / not started by harness" )
fi
if [[ -n "${ss_pid-}" ]] && kill -0 "$ss_pid" 2>/dev/null; then
  echo "PASS: SS process running (+2)"; score=$((score + 2))
else
  warn "SS not started by harness (no points)."; failed+=( "Spec: SS not running / not started by harness" )
fi
# Basic protocol docs presence check (small points)
if [[ -f "$BASE/docs/PROTOCOL.md" ]]; then
  echo "PASS: PROTOCOL.md present (+5)"; score=$((score + 5))
else
  warn "PROTOCOL.md missing"; failed+=( "Spec: PROTOCOL.md missing" )
fi

# ---------- Final summary ----------
h1 "GRADING COMPLETE"
echo "Final Score: $score / $TOTAL_POSSIBLE"

# Print failed tests for debugging
if (( ${#failed[@]} > 0 )); then
  h1 "FAILED TESTCASES (${#failed[@]})"
  for f in "${failed[@]}"; do
    echo " - $f"
  done
else
  echo "All tests passed or no failure detected. Nice!"
fi

echo
echo "Useful artifacts & tips:"
echo " - NM log: $NM_LOG"
echo " - SS log: $SS_LOG"
echo " - SS data dir: $SS_DATA_DIR"
echo "If tests fail due to CLI differences, edit the top configuration and the send_client() wrapper to match your exact client/server flags and protocol."

# exit 0 to avoid CI false-fails; change to non-zero if you want failing exit codes when tests fail
exit 0