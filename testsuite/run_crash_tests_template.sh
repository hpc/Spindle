#!/usr/bin/env bash

# Usage:
#   ./run_crash_tests.sh [--launcher=serial|flux|slurm|slurm-plugin] --nodes=N
#                        [--scratch=DIR] [--modes=LIST] [--session | --cross-exe]
#
# By default, runs all normal crash tests; use --modes to specify a subset to run.
# --session and --cross-exe instead run the session-based tests;
# these are separate because they require fresh sessions
#
# If running the tests from a non-shared filesystem, set --scratch to a shared
# filesystem so that the script can count all corefiles produced across all nodes.
# The CI containers' main filesystem is not shared, so a shared volume should be
# mounted across all containers and specified in --scratch

set -u

LAUNCHER="TEST_RESOURCE_MANAGER"
NODES=""
CRASH_TEST_SCRATCH="${CRASH_TEST_SCRATCH:-}"
SPINDLE="${SPINDLE:-SPINDLE_EXEC}"
SPINDLE_RC="${SPINDLE_RC:-SPINDLE_RC_PATH}"
TESTDIR="${TESTDIR:-TEST_RUN_DIR}"

die() { echo "FAIL: $*" >&2; exit 1; }

# The tests to run
# Fields:
#  mode: --mode to pass to crash test runner
#  cores: expected number of cores produced; if N, then equal to total number of ranks
#  crashers: expected number of crashing ranks
#    N = all ranks, E = even ranks (ceil(N/2)), or a literal count
#  flags (comma-separated): "multi-rank" skips the mode on a single rank;
#    "clean" expects the test to NOT crash; "altstack" runs the mode with
#    --crash-altstack.
#  top_frame_regex: regex that should match the top frame in produced coredumps.
#    Note that all threads will be checked, so in multithreaded examples the regex should
#    also match anything that could be on threads other than the one that faulted.
#  site_regex: optional regex for the site column of the crash log; 
#    if present, every logged site must match the regex for the test to pass
#  binary: optional alternate executable to run in place of the default crash_test.
#  crash_mode: optional alternate crash mode argument to executable
CRASH_TESTS=(
# mode                           ; cores ; crashers ; flags             ; top_frame_regex                              ; site_regex                                   ; binary               ; crash_mode
 'all-same                       ; 1     ; N        ;                   ; crash_function_A                             ; crash_test\+0x    '
 'fixed-address-exe              ; 1     ; N        ;                   ; crash_function_A                             ; crash_test_fixedaddr\+0x                     ; crash_test_fixedaddr ; all-same'
 'pie-exe                        ; 1     ; N        ;                   ; crash_function_A                             ; crash_test_pie\+0x                           ; crash_test_pie       ; all-same'
 'all-different                  ; N     ; N        ;                   ; crash_function_[0-9]+'
 'two-groups                     ; 2     ; N        ;                   ; crash_function_(A|B)'
 'one-crashes                    ; 1     ; 1        ;                   ; crash_function_A'
 'partial                        ; 1     ; E        ;                   ; crash_function_A'
 'late-straggler                 ; 1     ; N        ;                   ; crash_function_A'
 'in-library                     ; 1     ; N        ;                   ; crash_in_library                             ; libcrashfuncs\.so\+0x    '
 'in-dlmopen-library             ; 1     ; N        ;                   ; crash_in_library                             ; libcrashfuncs\.so\+0x    '
 'in-fixed-library               ; 1     ; N        ;                   ; crash_in_fixed_library                       ; libcrashfixed\.so\+0x    '
 'in-fixed-dlmopen-library       ; 1     ; N        ;                   ; crash_in_fixed_library                       ; libcrashfixed\.so\+0x    '
 'in-library-ctor                ; 1     ; 1        ;                   ; ctor_crash'
 'sigabrt                        ; 1     ; N        ;                   ; (__GI_)?raise|abort|pthread_kill'
 'assert                         ; 1     ; N        ;                   ; (__GI_)?raise|abort|pthread_kill             ; ^abort:.*Assertion    '
 'mixed-abort-segv               ; 2     ; N        ; multi-rank        ; (__GI_)?raise|abort|pthread_kill|do_mixed_abort_segv'
 'kill-segv                      ; 1     ; N        ;                   ; kill|do_kill_segv                            ; libc\.so.*\+0x    '
 'span-read                      ; 1     ; N        ;                   ; do_span_read'
 'safepoint                      ; 0     ; 0        ; clean             ; -'
 'safepoint-then-crash           ; 1     ; N        ;                   ; crash_function_A'
 'safepoint-bad                  ; 1     ; N        ;                   ; do_safepoint_bad'
 'safepoint-bad-write            ; 1     ; N        ;                   ; do_safepoint_bad_write'
 'safepoint-fix-write            ; 0     ; 0        ; clean             ; -'
 'safepoint-fix-write-altstack   ; 0     ; 0        ; clean,altstack    ; -                                            ;                                              ;                      ; safepoint-fix-write'
 'safepoint-longjmp              ; 0     ; 0        ; clean             ; -'
 'safepoint-span-read            ; 0     ; 0        ; clean             ; -'
 'safepoint-span-write           ; 0     ; 0        ; clean             ; -'
 'safepoint-span-bad-read        ; 1     ; N        ;                   ; do_safepoint_span_bad_read'
 'safepoint-span-bad-write       ; 1     ; N        ;                   ; do_safepoint_span_bad_write'
 'mmap-sigbus-bad                ; 1     ; N        ;                   ; do_mmap_sigbus_bad'
 'mmap-sigbus-fixed              ; 0     ; 0        ; clean             ; -'
 'chained-kill-segv              ; 0     ; 0        ; clean             ; -'
 'no-crash                       ; 0     ; 0        ; clean             ; -'
)

declare -A TEST_CORES TEST_CRASHERS TEST_FLAGS TEST_TOPFRAME TEST_SITE TEST_BINARY TEST_CRASHMODE
DEFAULT_MODES=()
SESSION=0
CROSS_EXE=0

# ---------------- test table parsing ----------------

trim() {
   local s="$1"
   s="${s#"${s%%[![:space:]]*}"}"
   s="${s%"${s##*[![:space:]]}"}"
   printf '%s' "$s"
}

parse_table() {
   local row mode cores crashers flags top site bin cmode
   for row in "${CRASH_TESTS[@]}"; do
      IFS=';' read -r mode cores crashers flags top site bin cmode <<<"$row"
      mode=$(trim "$mode")
      [ -n "$mode" ] || continue
      cores=$(trim "$cores")
      crashers=$(trim "$crashers")
      flags=$(trim "$flags")
      top=$(trim "$top")
      site=$(trim "$site")
      bin=$(trim "$bin")
      cmode=$(trim "$cmode")
      [ -n "$bin" ]   || bin="crash_test"
      [ -n "$cmode" ] || cmode="$mode"
      TEST_CORES[$mode]="$cores"
      TEST_CRASHERS[$mode]="$crashers"
      TEST_FLAGS[$mode]="$flags"
      TEST_TOPFRAME[$mode]="$top"
      TEST_SITE[$mode]="$site"
      TEST_BINARY[$mode]="$bin"
      TEST_CRASHMODE[$mode]="$cmode"
      DEFAULT_MODES+=("$mode")
   done
}

has_flag() {
   case ",${TEST_FLAGS[$1]:-}," in
      *,"$2",*) return 0 ;;
      *)        return 1 ;;
   esac
}

resolve_cores() {
   local mode="$1" val
   if [ "$LAUNCHER" = "serial" ]; then
      printf '1'
      return
   fi
   val="${TEST_CORES[$mode]}"
   [ "$val" = "N" ] && val="$NODES"
   printf '%s' "$val"
}

# Expected number of crashing ranks
resolve_crashers() {
   local mode="$1" val
   val="${TEST_CRASHERS[$mode]}"
   case "$val" in
      N) val="$NODES" ;;
      E) val=$(( (NODES + 1) / 2 )) ;;
   esac
   printf '%s' "$val"
}

# ---------------- arguments ----------------

usage() {
   local prog
   prog=$(basename "$0")
   cat <<EOF
Usage:
  $prog [--launcher=serial|flux|slurm|slurm-plugin] [--nodes=N]
        [--scratch=DIR] [--modes=mode1,mode2,...] [--session | --cross-exe]

Runs tests of the crash handler.

Options:
  --launcher=LAUNCHER  Resource manager to launch under: serial, flux, slurm, or
                       slurm-plugin.
  --nodes=N            Number of nodes/ranks to run on.
  --scratch=DIR        Directory where coredumps will be written.
                       When running on multiple nodes, this must be on a
                       shared filesystem.
  --modes=LIST         Comma-separated subset of modes to run (default: all).
  --session            Run the session mode crash log test instead of the
                       normal tests.
  --cross-exe          Run the cross-executable dedup test instead of the
                       normal tests.
  --help, -h           Show this help and exit.

Available tests:
EOF
   local mode
   for mode in "${DEFAULT_MODES[@]}"; do
      printf '  %s\n' "$mode"
   done
}

parse_args() {
   MODES=""
   for a in "$@"; do
      case "$a" in
         --launcher=*) LAUNCHER="${a#*=}" ;;
         --nodes=*)    NODES="${a#*=}"    ;;
         --scratch=*)  CRASH_TEST_SCRATCH="${a#*=}"  ;;
         --modes=*)    MODES="${a#*=}"    ;;
         --session)    SESSION=1 ;;
         --cross-exe)  CROSS_EXE=1 ;;
         --help|-h)    usage; exit 0 ;;
         *) die "unknown argument '$a'" ;;
      esac
   done
}

check_prereqs() {
   local cp
   cp=$(cat /proc/sys/kernel/core_pattern)
   if [[ "${cp:0:1}" == "|" ]]; then
      die "crash tests can't run because core_pattern is set to a pipe"
   fi
   if [[ "${cp:0:1}" == "/" ]]; then
      die "crash tests can't run because core_pattern is an absolute path"
   fi
   if [[ "$cp" != *"%p"* ]]; then
      die "crash tests can't run because core_pattern does not include pid"
   fi

   command -v gdb >/dev/null 2>&1 || \
      die "crash tests can't run because gdb is not on path"

   case "$LAUNCHER" in
      serial|flux|slurm|slurm-plugin) ;;
      *) die "unknown launcher" ;;
   esac

   if [ "$LAUNCHER" = "serial" ]; then
      NODES=1
   elif [ -z "$NODES" ]; then
      die "--nodes required"
   elif ! [[ "$NODES" =~ ^[1-9][0-9]*$ ]]; then
      die "--nodes must be a positive integer (was '$NODES')"
   fi

   test -x "$TESTDIR/crash_test"            || die "can't find crash test executable"
   test -x "$TESTDIR/crash_test_fixedaddr"  || die "can't find crash_test_fixedaddr executable"
   test -x "$TESTDIR/crash_test_pie"        || die "can't find crash_test_pie executable"
   test -f "$TESTDIR/libcrashfuncs.so"      || die "can't find libcrashfuncs.so"
   test -f "$TESTDIR/libcrashfixed.so"      || die "can't find libcrashfixed.so"
}

# ---------------- test launching helpers ----------------

launch() {
   local mode="$1"
   local logfile="$2"
   local binary="$TESTDIR/${TEST_BINARY[$mode]:-crash_test}"
   local crash_mode="${TEST_CRASHMODE[$mode]:-$mode}"
   local cmdline_opts="--crash-dedup --crash-log=$logfile"
   local flux_opts=(-o spindle.crash-dedup -o spindle.crash-log="$logfile")
   if has_flag "$mode" altstack; then
      cmdline_opts="$cmdline_opts --crash-altstack"
      flux_opts+=(-o spindle.crash-altstack)
   fi
   ulimit -c unlimited
   # Make libcrashfuncs.so visible to dlopen() from the mode's scratch dir.
   export LD_LIBRARY_PATH="$TESTDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
   case "$LAUNCHER" in
      serial)
         "$SPINDLE" \
            --no-mpi $cmdline_opts -- \
            "$binary" --crash-mode "$crash_mode"
         ;;
      flux)
         flux run \
            -o userrc="$SPINDLE_RC" \
            "${flux_opts[@]}" \
            -N"$NODES" -n"$NODES" \
            --env=LD_LIBRARY_PATH \
            -- "$binary" --crash-mode "$crash_mode"
         ;;
      slurm)
         salloc -N"$NODES" -n"$NODES" \
            "$SPINDLE" $cmdline_opts -- \
               srun "$binary" --crash-mode "$crash_mode"
         ;;
      slurm-plugin)
         salloc -N"$NODES" -n"$NODES" \
            srun --spindle="$cmdline_opts" \
               "$binary" --crash-mode "$crash_mode"
         ;;
   esac
}

# ---------------- coredump inspection ----------------

core_files() {
   local dir="$1"
   local cp glob search_dir
   cp=$(cat /proc/sys/kernel/core_pattern)
   case "$cp" in
      /*) glob=$(basename "$cp"); search_dir=$(dirname "$cp") ;;
      *)  glob="$cp";             search_dir="$dir" ;;
   esac
   # This converts any parameters (%p, %e, etc.) in the core pattern
   # into a shell glob so we can match all produced core files
   # (for example, "core.%e.%p" becomes "core.*.*")
   glob=$(echo "$glob" | sed -E 's|%[-0-9]*[peghistuclIPCsdfkrSTK]|*|g')
   find "$search_dir" -maxdepth 3 -type f -name "$glob" 2>/dev/null
}

count_cores() {
   core_files "$1" | wc -l
}

verify_top_frames() {
   local mode="$1"
   local dir="$2"
   local expected_top="${TEST_TOPFRAME[$mode]:-}"

   [ "$expected_top" = "-" ] && return 0
   [ -z "$expected_top" ] && return 0

   local binary="$TESTDIR/${TEST_BINARY[$mode]:-crash_test}"
   local core top cores
   cores=$(core_files "$dir")
   for core in $cores; do
      [ -e "$core" ] || continue
      top=$(gdb -batch -nx \
         -iex 'set print demangle off' \
         -ex 'set pagination off' \
         -ex 'bt 1' "$binary" "$core" 2>/dev/null \
         | grep -oE '#0\s+.*' | head -1)
      if ! echo "$top" | grep -qE "$expected_top"; then
         echo "   core $core: top frame '$top' does not match '$expected_top'" >&2
         return 1
      fi
   done
   return 0
}

# Read the crash site key (library+offset) out of the coredump
read_crash_site() {
   local core="$1"
   local binary="${2:-$TESTDIR/crash_test}"
   local map base cachepath real
   # Get the mappings from the coredump and extract the Spindle audit library
   map=$(gdb -batch -nx -ex 'set debuginfod enabled off' \
            -ex 'info proc mappings' "$binary" "$core" 2>/dev/null \
         | awk '/-spindlens-dso-libspindle_audit/ && $4 == "0x0" {print; exit}')
   base=$(printf '%s' "$map" | awk '{print $1}')
   cachepath=$(printf '%s' "$map" | awk '{print $NF}')
   # Figure out the path to the Spindle audit library
   real=$(printf '%s' "$cachepath" | sed -E 's#^.*/spindle\.[0-9a-f]+##; s#/[0-9]+-spindlens-dso-#/#')
   # Open the coredump with the Spindle audit library symbols loaded
   # so we can check crash_site_buf
   gdb -batch -nx -ex 'set debuginfod enabled off' \
      -ex "add-symbol-file $real -o $base" \
      -ex 'printf "CRASH_SITE=%s\n", crash_site_buf' \
      "$binary" "$core" 2>/dev/null \
      | sed -n 's/^CRASH_SITE=//p' | head -1
}

# ---------------- crash log parsing ----------------

# The crash log is CSV with the fields:
#  - rank
#  - exemplar
#  - exe
#  - site
#  - corepath

# Check that a crash log exists and starts with the expected header.
log_check_header() {
   local log="$1" header
   if [ ! -f "$log" ]; then
      echo "   missing crash log $log" >&2
      return 1
   fi
   IFS= read -r header <"$log"
   if [[ "$header" != rank,exemplar,exe,site,corepath* ]]; then
      echo "   incorrect crash log header '$header'" >&2
      return 1
   fi
   return 0
}

log_rows() { tail -n +2 "$1"; }

# Split a log entry into ROW_RANK, ROW_EXEMPLAR, ROW_EXE, ROW_SITE, ROW_COREPATH.
parse_log_row() {
   local rest
   IFS=, read -r ROW_RANK ROW_EXEMPLAR rest <<<"$1"
   ROW_EXE="${rest%%,*}"
   rest="${rest#*,}"
   # Remove quoting if present
   if [[ "$rest" =~ ^\"(([^\"]|\"\")*)\"(,(.*))?$ ]]; then
      ROW_SITE="${BASH_REMATCH[1]//\"\"/\"}"
      rest="${BASH_REMATCH[4]}"
   else
      ROW_SITE="${rest%%,*}"
      rest="${rest#"$ROW_SITE"}"
      rest="${rest#,}"
   fi
   ROW_COREPATH="${rest%%,*}"
}

# ---------------- crash log verification ----------------

# Verify the crash log file contains the expected crash sites.
verify_crash_log() {
   local mode="$1"
   local dir="$2"
   local expected_sites="$3"
   local log="$dir/crash.log"
   local expected_total
   expected_total=$(resolve_crashers "$mode")

   local expected_site="${TEST_SITE[$mode]:-}"
   [ "$expected_site" = "-" ] && expected_site=""

   local binary_name="${TEST_BINARY[$mode]:-crash_test}"
   local expected_exe="${binary_name}\$"

   log_check_header "$log" || return 1

   local rc=0 total=0 line key
   local -A seen=() site_exemplar=() exemplar_rows=()
   while IFS= read -r line; do
      parse_log_row "$line"
      key="$ROW_EXE|$ROW_SITE"
      total=$((total + 1))
      if ! [[ "$ROW_RANK" =~ ^[0-9]+$ ]] || [ "$ROW_RANK" -ge "$NODES" ]; then
         echo "   rank '$ROW_RANK' outside expected range [0,$NODES)" >&2
         rc=1
         continue
      fi
      if [ -n "${seen[$ROW_RANK]:-}" ]; then
         echo "   rank $ROW_RANK repeated" >&2
         rc=1
      fi
      seen[$ROW_RANK]=1
      if [ -z "${site_exemplar[$key]:-}" ]; then
         site_exemplar[$key]="$ROW_EXEMPLAR"
         if ! [[ "$ROW_EXE" =~ $expected_exe ]]; then
            echo "   exe '$ROW_EXE' does not match executable '$binary_name'" >&2
            rc=1
         fi
         if [ -n "$expected_site" ] && ! [[ "$ROW_SITE" =~ $expected_site ]]; then
            echo "   site '$ROW_SITE' does not match '$expected_site'" >&2
            rc=1
         fi
      elif [ "${site_exemplar[$key]}" != "$ROW_EXEMPLAR" ]; then
         echo "   site '$key' exemplar ${site_exemplar[$key]} does not match expected $ROW_EXEMPLAR" >&2
         rc=1
      fi
      if [ "$ROW_RANK" = "$ROW_EXEMPLAR" ]; then
         exemplar_rows[$key]=$(( ${exemplar_rows[$key]:-0} + 1 ))
      fi
   done < <(log_rows "$log")

   for key in "${!site_exemplar[@]}"; do
      if [ "${exemplar_rows[$key]:-0}" != "1" ]; then
         echo "   exemplar ${site_exemplar[$key]} of site '$key' appears in ${exemplar_rows[$key]:-0} rows, expected 1" >&2
         rc=1
      fi
   done

   if [ "${#site_exemplar[@]}" != "$expected_sites" ]; then
      echo "   ${#site_exemplar[@]} crashsites, expected $expected_sites" >&2
      rc=1
   fi
   if [ "$total" != "$expected_total" ]; then
      echo "   $total total ranks, expected $expected_total" >&2
      rc=1
   fi
   return $rc
}

# Verify that each coredump recorded in the log actually exists on disk.
verify_exemplar_cores() {
   local mode="$1"
   local dir="$2"
   local log="$dir/crash.log"

   local -A rank_pid
   local line r p f
   for f in "$dir/stdout.log" "$dir/stderr.log"; do
      [ -f "$f" ] || continue
      while IFS= read -r line; do
         if [[ "$line" =~ rank=([0-9]+)\ .*pid=([0-9]+) ]]; then
            r="${BASH_REMATCH[1]}"
            p="${BASH_REMATCH[2]}"
            rank_pid[$r]="$p"
         fi
      done <"$f"
   done

   local core cores
   cores=$(core_files "$dir")

   local ex found predicted
   local -A checked=()
   while IFS= read -r line; do
      parse_log_row "$line"
      ex="$ROW_EXEMPLAR"
      predicted="$ROW_COREPATH"
      [ -z "${checked[$ex]:-}" ] || continue
      checked[$ex]=1
      p="${rank_pid[$ex]:-}"
      if [ -z "$p" ]; then
         echo "   no pid banner for exemplar rank $ex; skipping core check" >&2
         continue
      fi
      found=0
      for core in $cores; do
         case "$core" in
            */rank_"$ex"/*) ;;
            *) continue ;;
         esac
         if [[ "${core##*/}" =~ (^|[^0-9])$p([^0-9]|$) ]]; then
            found=1
            break
         fi
      done
      if [ "$found" != "1" ]; then
         echo "   no coredump found for exemplar rank $ex (pid $p)" >&2
         return 1
      fi
      # The log's corepath is the exemplar's predicted core file; it must be
      # the file that was actually written
      if [ "$predicted" != "$core" ]; then
         echo "   logged corepath '$predicted' != coredump '$core' for exemplar rank $ex" >&2
         return 1
      fi
   done < <(log_rows "$log")
   return 0
}

# Verify that logged crash site matches what is recorded in the coredump.
verify_log_matches_core() {
   local mode="$1"
   local dir="$2"
   local log="$dir/crash.log"
   local binary="$TESTDIR/${TEST_BINARY[$mode]:-crash_test}"

   local core core_site
   core=$(core_files "$dir" | head -1)
   [ -n "$core" ] || { echo "   no core for gdb cross-check" >&2; return 1; }
   parse_log_row "$(log_rows "$log" | head -1)"
   core_site=$(read_crash_site "$core" "$binary")
   if [ -z "$core_site" ]; then
      echo "   could not read crash site from $core" >&2
      return 1
   fi
   if [ "$core_site" != "$ROW_SITE" ]; then
      echo "   core site '$core_site' != logged site '$ROW_SITE'" >&2
      return 1
   fi
   return 0
}

# ---------------- session tests ----------------

# Common setup for the session-based tests.  Sets three globals consumed by
# session_test_launch and the run_*_test functions:
#   SESSION_DIR  per-test scratch directory
#   SESSION_LOG  crash log path inside SESSION_DIR
#   SESSION_RUN  launcher command that runs one program inside the session
session_test_setup() {
   local name="$1"
   case "$LAUNCHER" in
      slurm-plugin|flux) ;;
      *) die "--$name requires --launcher=slurm-plugin or --launcher=flux" ;;
   esac

   mkdir -p "$CRASH_TEST_SCRATCH" || die "can't create scratch dir '$CRASH_TEST_SCRATCH'"
   SESSION_DIR=$(mktemp -d "$CRASH_TEST_SCRATCH/$name.XXXXXX") || die "can't create test dir"
   SESSION_LOG="$SESSION_DIR/crash.log"

   case "$LAUNCHER" in
      slurm-plugin)
         SESSION_RUN="srun --spindle"
         ;;
      flux)
         SESSION_RUN="flux run -o userrc=$SPINDLE_RC -o spindle --env=LD_LIBRARY_PATH -N$NODES -n$NODES --"
         ;;
   esac
}

session_test_launch() {
   local session_opts="--crash-dedup --crash-log=$SESSION_LOG"
   chmod +x "$SESSION_DIR/inner.sh"
   ulimit -c unlimited
   case "$LAUNCHER" in
      slurm-plugin)
         ( cd "$SESSION_DIR" && salloc -N"$NODES" -n"$NODES" \
              --spindle-session="$session_opts" "$SESSION_DIR/inner.sh" ) \
            >"$SESSION_DIR/stdout.log" 2>"$SESSION_DIR/stderr.log"
         ;;
      flux)
         # The session's crash options must be given at session start
         local sid
         sid=$("$SPINDLE" --start-session $session_opts 2>"$SESSION_DIR/session.log") || \
            die "spindle --start-session failed (see $SESSION_DIR/session.log)"
         ( cd "$SESSION_DIR" && "$SESSION_DIR/inner.sh" ) \
            >"$SESSION_DIR/stdout.log" 2>"$SESSION_DIR/stderr.log"
         if [ -n "$sid" ]; then
            "$SPINDLE" --end-session="$sid" >>"$SESSION_DIR/session.log" 2>&1
         else
            "$SPINDLE" --end-session >>"$SESSION_DIR/session.log" 2>&1
         fi
         ;;
   esac
   # Wait briefly for the server to shut down and write the log
   sleep 2
}

# Session-mode crash-log test
# two crashing runs inside one spindle session share a crash log.
run_session_test() {
   session_test_setup session
   local dir="$SESSION_DIR" log="$SESSION_LOG"

   # inner.sh is the script that gets run inside the session
   cat >"$dir/inner.sh" <<EOF
#!/bin/bash
export LD_LIBRARY_PATH="$TESTDIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
$SESSION_RUN "$TESTDIR/crash_test" --crash-mode all-same
sleep 3
if [ -e "$log" ]; then echo present; else echo absent; fi > "$dir/log_after_run1"
$SESSION_RUN "$TESTDIR/crash_test" --crash-mode sigabrt
sleep 3
if [ -e "$log" ]; then echo present; else echo absent; fi > "$dir/log_after_run2"
EOF
   session_test_launch

   local after1 after2 sites=0 total=0 line
   local -A keys=()
   after1=$(cat "$dir/log_after_run1" 2>/dev/null || echo missing)
   after2=$(cat "$dir/log_after_run2" 2>/dev/null || echo missing)
   if log_check_header "$log" 2>/dev/null; then
      while IFS= read -r line; do
         parse_log_row "$line"
         keys["$ROW_EXE|$ROW_SITE"]=1
         total=$((total + 1))
      done < <(log_rows "$log")
      sites=${#keys[@]}
   fi

   local ok=1
   [ "$after1" = "absent" ] || { echo "FAIL session: log $after1 after run 1"; ok=0; }
   [ "$after2" = "absent" ] || { echo "FAIL session: log $after2 after run 2"; ok=0; }
   [ "$sites" = "2" ] || { echo "FAIL session: $sites sites after session end"; ok=0; }
   [ "$total" = "$((2 * NODES))" ] || \
      { echo "FAIL session: $total total ranks in final log (expected $((2 * NODES)))"; ok=0; }

   [ "$ok" = "1" ] || exit 1
   echo "PASS session"
}

# Cross-executable dedup test: two different executables crashing
# at the same offset in the same shared library inside one session
# should not be deduplicated
run_cross_exe_test() {
   session_test_setup cross-exe
   local dir="$SESSION_DIR"

   # inner.sh is the script that gets run inside the session
   cat >"$dir/inner.sh" <<EOF
#!/bin/bash
export LD_LIBRARY_PATH="$TESTDIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
$SESSION_RUN "$TESTDIR/crash_test" --crash-mode in-library
sleep 3
$SESSION_RUN "$TESTDIR/crash_test_pie" --crash-mode in-library
sleep 3
EOF
   session_test_launch

   verify_cross_exe "$dir" || exit 1
   echo "PASS cross-exe"
}

verify_cross_exe() {
   local dir="$1"
   local log="$dir/crash.log"

   local ncores sites ok=1 line key i
   local -a exes=() tails=() counts=()
   local -A index=()
   ncores=$(count_cores "$dir")
   # Collect the crash sites
   if log_check_header "$log" 2>/dev/null; then
      while IFS= read -r line; do
         parse_log_row "$line"
         key="$ROW_EXE|$ROW_SITE"
         i="${index[$key]:-}"
         if [ -z "$i" ]; then
            i=${#exes[@]}
            index[$key]=$i
            exes+=("$ROW_EXE")
            tails+=("$ROW_SITE")
            counts+=(0)
         fi
         counts[i]=$((counts[i] + 1))
      done < <(log_rows "$log")
   fi
   sites=${#exes[@]}

   [ "$ncores" = "2" ] || { echo "FAIL cross-exe: $ncores coredumps (expected 2)"; ok=0; }
   [ "$sites" = "2" ] || { echo "FAIL cross-exe: $sites crashsites (expected 2)"; ok=0; }

   if [ "$sites" = "2" ]; then
      # Same library site under both programs
      local lib_regex='libcrashfuncs\.so\+0x'
      if ! [[ "${tails[0]}" =~ $lib_regex ]] || ! [[ "${tails[1]}" =~ $lib_regex ]]; then
         echo "FAIL cross-exe: crash sites are not both in libcrashfuncs.so ('${tails[0]}', '${tails[1]}')"
         ok=0
      elif [ "${tails[0]}" != "${tails[1]}" ]; then
         echo "FAIL cross-exe: crash sites unexpectedly differ ('${tails[0]}' vs '${tails[1]}')"
         ok=0
      fi
      # distinguished by the executable part of the crash key
      if [ "${exes[0]}" = "${exes[1]}" ]; then
         echo "FAIL cross-exe: both crash sites name the same executable '${exes[0]}'"
         ok=0
      fi
      local pie_regex='crash_test_pie$' plain_regex='crash_test$'
      if ! { [[ "${exes[0]}" =~ $plain_regex ]] && [[ "${exes[1]}" =~ $pie_regex ]]; } && \
         ! { [[ "${exes[1]}" =~ $plain_regex ]] && [[ "${exes[0]}" =~ $pie_regex ]]; }; then
         echo "FAIL cross-exe: wrong crash sites found ('${exes[0]}', '${exes[1]}')"
         ok=0
      fi
      # Verify each program crashed on every node, and the two runs did not merge.
      local c
      for c in "${counts[@]}"; do
         [ "$c" = "$NODES" ] || { echo "FAIL cross-exe: crashsite has $c rows (expected $NODES)"; ok=0; }
      done
   fi

   [ "$ok" = "1" ]
}

# ---------------- test verification helpers ----------------

# A "clean" mode must exit 0 with no coredumps and no crash log.
verify_clean_mode() {
   local mode="$1" dir="$2" launch_rc="$3"
   local actual_cores
   actual_cores=$(count_cores "$dir")
   if [ "$actual_cores" != "0" ]; then
      echo "FAIL $mode: expected 0 dumps, got $actual_cores"
      return 1
   fi
   if [ "$launch_rc" != "0" ]; then
      echo "FAIL $mode: launcher exited $launch_rc (expected 0)"
      return 1
   fi
   # No crashes means no crash log
   if [ -e "$dir/crash.log" ]; then
      echo "FAIL $mode: crash log unexpectedly written"
      return 1
   fi
   echo "PASS $mode (clean exit)"
}

# A crashing mode must produce the expected coredumps and a crash log
# that agrees with them.
verify_crashed_mode() {
   local mode="$1" dir="$2"
   local want actual
   want=$(resolve_cores "$mode")
   actual=$(count_cores "$dir")

   if [ "$actual" != "$want" ]; then
      echo "FAIL $mode: expected $want coredumps, got $actual"
      return 1
   fi

   # Verify that the core files show the expected crash sites
   if ! verify_top_frames "$mode" "$dir"; then
      echo "FAIL $mode: coredump top-frame verification failed"
      return 1
   fi

   # Verify sites, ranks, counts, and exemplars from the crash log
   if ! verify_crash_log "$mode" "$dir" "$actual"; then
      echo "FAIL $mode: crash log verification failed"
      return 1
   fi

   # Verify each exemplar rank owns a coredump on disk
   if ! verify_exemplar_cores "$mode" "$dir"; then
      echo "FAIL $mode: exemplar recorded in log does not correspond to a coredump file"
      return 1
   fi

   # Check the coredump's recorded crash site against the log
   if [ "$LAUNCHER" = "serial" ] && [ "$mode" = "all-same" ]; then
      if ! verify_log_matches_core "$mode" "$dir"; then
         echo "FAIL $mode: gdb check of crash site does not match log"
         return 1
      fi
   fi

   echo "PASS $mode ($actual dumps)"
}

# Run one sweep mode in its own scratch directory and verify the results.
# Prints the mode's PASS/FAIL line; returns 0 iff the mode passed.
run_one_mode() {
   local mode="$1"

   # Run each test in a per-test directory so all its coredumps land in
   # one place and we can count them
   mkdir -p "$CRASH_TEST_SCRATCH" || die "can't create scratch dir '$CRASH_TEST_SCRATCH'"
   local dir
   dir=$(mktemp -d "$CRASH_TEST_SCRATCH/$mode.XXXXXX") || die "can't create test dir under '$CRASH_TEST_SCRATCH'"
   local launch_rc=0
   ( cd "$dir" && launch "$mode" "$dir/crash.log" ) >"$dir/stdout.log" 2>"$dir/stderr.log" || launch_rc=$?
   [ "$LAUNCHER" = "serial" ] || sleep 1

   if has_flag "$mode" clean; then
      verify_clean_mode "$mode" "$dir" "$launch_rc"
   else
      verify_crashed_mode "$mode" "$dir"
   fi
}

main() {
   parse_table
   parse_args "$@"

   if [ -z "$CRASH_TEST_SCRATCH" ]; then
      if [ -n "${SPINDLE_TEST_CONTAINER:-}" ]; then
         die "--scratch=DIR is required when running in a CI container"
      fi
      CRASH_TEST_SCRATCH="$TESTDIR/spindle_crash_test"
   fi
   check_prereqs

   if [ "$SESSION" = "1" ]; then
      run_session_test
      return
   fi

   if [ "$CROSS_EXE" = "1" ]; then
      run_cross_exe_test
      return
   fi

   local pass=0 fail=0

   local modes_to_run
   if [ -z "$MODES" ]; then
      modes_to_run=("${DEFAULT_MODES[@]}")
   else
      modes_to_run=()
      IFS=',' read -ra specified_modes <<< "$MODES"
      for m in "${specified_modes[@]}"; do
         modes_to_run+=("$(trim "$m")")
      done
   fi

   for mode in "${modes_to_run[@]}"; do
      if [ -z "${TEST_CORES[$mode]+set}" ]; then
         die "unknown mode '$mode'"
      fi

      if has_flag "$mode" multi-rank && \
         { [ "$LAUNCHER" = "serial" ] || [ "$NODES" -lt 2 ]; }; then
         echo "SKIP $mode (needs multiple ranks)"
         continue
      fi

      if run_one_mode "$mode"; then
         pass=$((pass+1))
      else
         fail=$((fail+1))
      fi
   done

   echo
   echo "Summary: $pass passed, $fail failed"
   [ "$fail" -eq 0 ] || exit 1
}

main "$@"
