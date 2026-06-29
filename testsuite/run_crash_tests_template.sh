#!/usr/bin/env bash

# Usage:
#   ./run_crash_tests.sh [--launcher=serial|flux|slurm|slurm-plugin] --nodes=N [--scratch=DIR]
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
#  flags: if "multi-rank", skip when running on only one rank; if "clean", expect test to NOT crash
#  top_frame_regex: regex that should match the top frame in produced coredumps.
#    Note that all threads will be checked, so in multithreaded examples the regex should
#    also match anything that could be on threads other than the one that faulted.
#  site_regex: optional regex for the crash-site dedup key; if present, the crash site key must
#    match the regex for the test to pass
#  binary: optional alternate executable to run in place of the default crash_test.
#  crash_mode: optional alternate crash mode argument to executable
CRASH_TESTS=(
# mode                           ; cores ; flags             ; top_frame_regex                              ; site_regex                                   ; binary               ; crash_mode
 'all-same                       ; 1     ;                   ; crash_function_A                             ; crash_test\+0x'
 'fixed-address-exe              ; 1     ;                   ; crash_function_A                             ; crash_test_fixedaddr\+0x                     ; crash_test_fixedaddr ; all-same'
 'pie-exe                        ; 1     ;                   ; crash_function_A                             ; crash_test_pie\+0x                           ; crash_test_pie       ; all-same'
 'all-different                  ; N     ;                   ; crash_function_[0-9]+'
 'two-groups                     ; 2     ;                   ; crash_function_(A|B)'
 'one-crashes                    ; 1     ;                   ; crash_function_A'
 'partial                        ; 1     ;                   ; crash_function_A'
 'late-straggler                 ; 1     ;                   ; crash_function_A'
 'in-library                     ; 1     ;                   ; crash_in_library                             ; libcrashfuncs\.so\+0x'
 'in-dlmopen-library             ; 1     ;                   ; crash_in_library                             ; libcrashfuncs\.so\+0x'
 'in-fixed-library               ; 1     ;                   ; crash_in_fixed_library                       ; libcrashfixed\.so\+0x'
 'in-fixed-dlmopen-library       ; 1     ;                   ; crash_in_fixed_library                       ; libcrashfixed\.so\+0x'
 'in-library-ctor                ; 1     ;                   ; ctor_crash'
 'sigabrt                        ; 1     ;                   ; (__GI_)?raise|abort|pthread_kill'
 'assert                         ; 1     ;                   ; (__GI_)?raise|abort|pthread_kill             ; abort:.*Assertion'
 'mixed-abort-segv               ; 2     ; multi-rank        ; (__GI_)?raise|abort|pthread_kill|do_mixed_abort_segv'
 'span-read                      ; 1     ;                   ; do_span_read'
 'safepoint                      ; 0     ; clean             ; -'
 'safepoint-then-crash           ; 1     ;                   ; crash_function_A'
 'safepoint-bad                  ; 1     ;                   ; do_safepoint_bad'
 'safepoint-bad-write            ; 1     ;                   ; do_safepoint_bad_write'
 'safepoint-fix-write            ; 0     ; clean             ; -'
 'safepoint-longjmp              ; 0     ; clean             ; -'
 'safepoint-span-read            ; 0     ; clean             ; -'
 'safepoint-span-write           ; 0     ; clean             ; -'
 'safepoint-span-bad-read        ; 1     ;                   ; do_safepoint_span_bad_read'
 'safepoint-span-bad-write       ; 1     ;                   ; do_safepoint_span_bad_write'
 'no-crash                       ; 0     ; clean             ; -'
)

declare -A TEST_CORES TEST_FLAGS TEST_TOPFRAME TEST_SITE TEST_BINARY TEST_CRASHMODE
DEFAULT_MODES=()

trim() {
   local s="$1"
   s="${s#"${s%%[![:space:]]*}"}"
   s="${s%"${s##*[![:space:]]}"}"
   printf '%s' "$s"
}

parse_table() {
   local row mode cores flags top site bin cmode
   for row in "${CRASH_TESTS[@]}"; do
      IFS=';' read -r mode cores flags top site bin cmode <<<"$row"
      mode=$(trim "$mode")
      [ -n "$mode" ] || continue
      cores=$(trim "$cores")
      flags=$(trim "$flags")
      top=$(trim "$top")
      site=$(trim "$site")
      bin=$(trim "$bin")
      cmode=$(trim "$cmode")
      [ -n "$bin" ]   || bin="crash_test"
      [ -n "$cmode" ] || cmode="$mode"
      TEST_CORES[$mode]="$cores"
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

usage() {
   local prog
   prog=$(basename "$0")
   cat <<EOF
Usage:
  $prog [--launcher=serial|flux|slurm|slurm-plugin] [--nodes=N]
        [--scratch=DIR] [--modes=mode1,mode2,...]

Runs tests of the crash handler.

Options:
  --launcher=LAUNCHER  Resource manager to launch under: serial, flux, slurm, or
                       slurm-plugin. 
  --nodes=N            Number of nodes/ranks to run on.
  --scratch=DIR        Directory where coredumps will be written.
                       When running on multiple nodes, this must be on a 
                       shared filesystem.
  --modes=LIST         Comma-separated subset of modes to run (default: all).
  --help, -h           Show this help and exit.

Available tests:
EOF
   local row mode cores flags top
   for row in "${CRASH_TESTS[@]}"; do
      IFS=';' read -r mode cores flags top <<<"$row"
      mode=$(trim "$mode")
      [ -n "$mode" ] || continue
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

launch() {
   local mode="$1"
   local binary="$TESTDIR/${TEST_BINARY[$mode]:-crash_test}"
   local crash_mode="${TEST_CRASHMODE[$mode]:-$mode}"
   ulimit -c unlimited
   # Make libcrashfuncs.so visible to dlopen() from the mode's scratch dir.
   export LD_LIBRARY_PATH="$TESTDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
   case "$LAUNCHER" in
      serial)
         "$SPINDLE" \
            --no-mpi --crash-dedup -- \
            "$binary" --crash-mode "$crash_mode"
         ;;
      flux)
         flux run \
            -o userrc="$SPINDLE_RC" \
            -o spindle.crash-dedup \
            -N"$NODES" -n"$NODES" \
            --env=LD_LIBRARY_PATH \
            -- "$binary" --crash-mode "$crash_mode"
         ;;
      slurm)
         salloc -N"$NODES" -n"$NODES" \
            "$SPINDLE" --crash-dedup -- \
               srun "$binary" --crash-mode "$crash_mode"
         ;;
      slurm-plugin)
         salloc -N"$NODES" -n"$NODES" \
            srun --spindle="--crash-dedup" \
               "$binary" --crash-mode "$crash_mode"
         ;;
   esac
}

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

# Check that the crash site key matches the expectation
# to verify that Spindle identified the correct library
verify_crash_site() {
   local mode="$1"
   local dir="$2"
   local expected_site="${TEST_SITE[$mode]:-}"

   # If this test doesn't have a crash site specified, there's nothing to do
   [ "$expected_site" = "-" ] && return 0
   [ -z "$expected_site" ] && return 0

   local binary="$TESTDIR/${TEST_BINARY[$mode]:-crash_test}"
   local core cores site
   cores=$(core_files "$dir")
   for core in $cores; do
      site=$(read_crash_site "$core" "$binary")
      if [ -z "$site" ]; then
         echo "   core $core: could not read crash site" >&2
         return 1
      fi
      if ! printf '%s' "$site" | grep -qE -- "$expected_site"; then
         echo "   core $core: crash dedup key '$site' does not match '$expected_site'" >&2
         return 1
      fi
   done
   return 0
}

main() {
   parse_args "$@"
   parse_table

   if [ -z "$CRASH_TEST_SCRATCH" ]; then
      if [ -n "${SPINDLE_TEST_CONTAINER:-}" ]; then
         die "--scratch=DIR is required when running in a CI container"
      fi
      CRASH_TEST_SCRATCH="$TESTDIR/spindle_crash_test"
   fi
   check_prereqs

   local pass=0 fail=0

   local modes_to_run
   if [ -z "$MODES" ]; then
      modes_to_run=("${DEFAULT_MODES[@]}")
   else
      modes_to_run=()
      IFS=',' read -ra specified_modes <<< "$MODES"
      for m in "${specified_modes[@]}"; do
         m=$(echo "$m" | xargs)
         modes_to_run+=("$m")
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

      # Run each test in per-test directory so all the coredumps run in one place
      # and we can count them
      mkdir -p "$CRASH_TEST_SCRATCH" || die "can't create scratch dir '$CRASH_TEST_SCRATCH'"
      local dir
      dir=$(mktemp -d "$CRASH_TEST_SCRATCH/$mode.XXXXXX") || die "can't create test dir under '$CRASH_TEST_SCRATCH'"
      local launch_rc=0
      ( cd "$dir" && launch "$mode" ) >"$dir/stdout.log" 2>"$dir/stderr.log" || launch_rc=$?
      [ "$LAUNCHER" = "serial" ] || sleep 1

      # If this test is not supposed to crash, verify that it didn't crash
      # and exited normally
      if has_flag "$mode" clean; then
         local actual_cores
         actual_cores=$(count_cores "$dir")
         if [ "$actual_cores" != "0" ]; then
            echo "FAIL $mode: expected 0 dumps, got $actual_cores"
            fail=$((fail+1))
            continue
         fi
         if [ "$launch_rc" != "0" ]; then
            echo "FAIL $mode: launcher exited $launch_rc (expected 0)"
            fail=$((fail+1))
            continue
         fi
         echo "PASS $mode (clean exit)"
         pass=$((pass+1))
         continue
      fi

      # Otherwise, if this test is supposed to crash, verify that we
      # got the number of core files that we expect
      local want
      want=$(resolve_cores "$mode")
      local actual
      actual=$(count_cores "$dir")

      if [ "$actual" != "$want" ]; then
         echo "FAIL $mode: expected $want coredumps, got $actual"
         fail=$((fail+1))
         continue
      fi

      # And verify that the core files show the expected crash sites
      if ! verify_top_frames "$mode" "$dir"; then
         echo "FAIL $mode: dump top-frame verification failed"
         fail=$((fail+1))
         continue
      fi

      # Verify the crash handler's <library>+<offset> dedup key
      if ! verify_crash_site "$mode" "$dir"; then
         echo "FAIL $mode: crash site dedup key verification failed"
         fail=$((fail+1))
         continue
      fi

      echo "PASS $mode ($actual dumps)"
      pass=$((pass+1))
   done

   echo
   echo "Summary: $pass passed, $fail failed"
   [ "$fail" -eq 0 ] || exit 1
}

main "$@"
