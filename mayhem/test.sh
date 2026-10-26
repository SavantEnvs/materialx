#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"
T="${TMPDIR:-/tmp}"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Upstream Catch2 suite (built by build.sh with normal flags), run from its data-bearing bin dir.
BIN="$SRC/build/tests/bin"
RUNNER="$BIN/MaterialXTest"
if [ ! -x "$RUNNER" ]; then
  echo "missing $RUNNER — build.sh should have produced it" >&2
  emit_ctrf "catch2+kat" 0 1 0
  exit 1
fi
LOG="$T/materialx-tests.log"
( cd "$BIN" && "$RUNNER" ) > "$LOG" 2>&1
tail -n 4 "$LOG"

passed=0; failed=0; skipped=0
if grep -Eq '^All tests passed \(' "$LOG"; then
  passed=$(sed -nE 's/^All tests passed \([0-9]+ assertions? in ([0-9]+) test cases?\).*/\1/p' "$LOG" | tail -1)
else
  line=$(grep -E '^test cases:' "$LOG" | tail -1)
  passed=$(printf '%s' "$line" | sed -nE 's/.*\| *([0-9]+) passed.*/\1/p')
  failed=$(printf '%s' "$line" | sed -nE 's/.*\| *([0-9]+) failed.*/\1/p')
  skipped=$(printf '%s' "$line" | sed -nE 's/.*\| *([0-9]+) skipped.*/\1/p')
fi
passed=${passed:-0}; failed=${failed:-0}; skipped=${skipped:-0}
# A runner that printed nothing (neutered, or the suite vanished) must count as a failure.
if [ "$passed" -lt 50 ]; then
  echo "suite reported only $passed passing test cases (expected ~99)" >&2
  failed=$((failed + 1))
fi

# Known-answer probes through a dynamically linked binary (the runner is judged by its own summary
# line above; these assert exact computed values from fixed inputs).
KAT_OUT="$T/materialx-kat.out"
"$SRC/mx_kat" "$T/materialx-kat.obj" > "$KAT_OUT" 2>"$T/materialx-kat.err"
cat "$KAT_OUT"
for want in \
  'version=1.39' 'nodes=3' 'nodedefs=1' 'c1.value=0.25, 0.5, 0.75' 'm.connected=c1' 'img.file=a.png' \
  'roundtrip.nodes=3' 'stdlib.nodedefs>100=1' 'valid=1' 'renderables=1' \
  'genglsl.nonempty=1' 'genglsl.has_0.2=1' 'genosl.nonempty=1' 'genosl.has_0.2=1' \
  'obj.ok=1 meshes=1' 'obj.faces=1' 'obj.maxy=2'; do
  if grep -qxF -- "$want" "$KAT_OUT"; then passed=$((passed + 1)); else echo "KAT MISMATCH: expected line '$want'" >&2; failed=$((failed + 1)); fi
done

emit_ctrf "catch2+kat" "$passed" "$failed" "$skipped"
