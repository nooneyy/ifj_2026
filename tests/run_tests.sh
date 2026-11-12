#!/usr/bin/env bash
#
# @file run_tests.sh
# @brief Runs the test suites from tests/<suite>/
# @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
# @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
# @date 12.11.2026
#
# Disclaimer!
# This testing file was made by Claude Opus 5.5 AI model in xHigh effort mode
# This file is only for testing purposes.
# Prompt:
#  Create a testing bash script for the scanner that makes the project, runs scanner_main with one
#   .ifj at a time as argv[1], compares the output with respective .out file and checks the return code
#   and does so with each .ifj and .out file which the script separates into sections. The script's
#   output should be whether the test ran correctly and in case it didnt then the script should show
#   the differences. The script will also run valgrind for memory leaks check.
#
# Disclaimer!
# Generalized from run_scanner_tests.sh by Claude Opus 5.5 AI model
# Prompt:
#  "As for the runner, I would change run_scanner_tests.sh to be a more general
#   script runner like run_tests.sh and take an argument as to which part we're
#   testing"
#
# Builds the project, then for every suite given as an argument
# and every NAME.ifj in tests/<suite>/:
#   1. runs tests/<suite>_main with NAME.ifj as argv[1]
#   2. compares its output with NAME.out
#   3. compares its return code with NAME.code (expects 0 if there's none)
#   4. runs it again under valgrind to find memory errors and leaks
# Every test gets its own section with PASS/FAIL for each part,
# the differences are shown in case of a failure.
# A suite is built by the Makefile target <suite>_test.
#
# Usage: tests/run_tests.sh <suite>...
#        e.g. tests/run_tests.sh scanner parser
#
# Return: 0 if every test passed
#         1 if at least one test failed
#         2 if the project couldn't be built or the arguments are wrong

set -u
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# valgrind's return code when it finds an error or a leak,
# can't collide with the compiler's return codes (0-10, 99)
VALGRIND_ERR=200

# colors only when printing into a terminal
if [ -t 1 ]; then
  GREEN=$'\e[32m'
  RED=$'\e[31m'
  YELLOW=$'\e[33m'
  BOLD=$'\e[1m'
  RESET=$'\e[0m'
else
  GREEN=''
  RED=''
  YELLOW=''
  BOLD=''
  RESET=''
fi

# result of one part of a test: name of the part, optional detail
pass() { printf '  %-12s %sPASS%s %s\n' "$1" "$GREEN" "$RESET" "${2:-}"; }
fail() { printf '  %-12s %sFAIL%s %s\n' "$1" "$RED" "$RESET" "${2:-}"; }
skip() { printf '  %-12s %sSKIP%s %s\n' "$1" "$YELLOW" "$RESET" "${2:-}"; }

# indents details, so they stay visually inside their section
indent() { sed 's/^/      /'; }

# ---------- arguments ----------
if [ $# -eq 0 ]; then
  printf 'Usage: %s <suite>...  (e.g. scanner parser)\n' "$0"
  exit 2
fi

suites=("$@")
targets=()
for suite in "${suites[@]}"; do
  if [ ! -d "$SCRIPT_DIR/$suite" ]; then
    printf '%sUnknown suite %s, tests/%s/ does not exist.%s\n' \
      "$RED" "$suite" "$suite" "$RESET"
    exit 2
  fi
  targets+=("${suite}_test")
done

# ---------- build ----------
printf '%sBuilding the project and %s...%s\n' "$BOLD" "${targets[*]}" "$RESET"
if ! make -s -C "$ROOT_DIR" all "${targets[@]}"; then
  printf '%sBuild failed, no tests were run.%s\n' "$RED" "$RESET"
  exit 2
fi

has_valgrind=true
if ! command -v valgrind > /dev/null 2>&1; then
  has_valgrind=false
  printf '%svalgrind not found, memory checks will be skipped.%s\n' \
    "$YELLOW" "$RESET"
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

shopt -s nullglob

passed=0
failed=0
failed_names=()

# runs every test of one suite, updates passed, failed and failed_names
run_suite() {
  local suite="$1"
  local test_dir="$SCRIPT_DIR/$suite"
  local test_main="$SCRIPT_DIR/${suite}_main"
  local tests=("$test_dir"/*.ifj)

  printf '\n%s########## suite: %s ##########%s\n' "$BOLD" "$suite" "$RESET"
  if [ ${#tests[@]} -eq 0 ]; then
    printf '%sNo .ifj files found in %s%s\n' "$RED" "$test_dir" "$RESET"
    failed=$((failed + 1))
    failed_names+=("$suite/(no tests)")
    return
  fi

  local ifj name out_file code_file actual errors test_ok code expected_code
  for ifj in "${tests[@]}"; do
    name="$(basename "$ifj" .ifj)"
    out_file="$test_dir/$name.out"
    code_file="$test_dir/$name.code"
    actual="$tmp_dir/$suite.$name.actual"
    errors="$tmp_dir/$suite.$name.stderr"
    test_ok=true

    printf '\n%s========== %s/%s ==========%s\n' "$BOLD" "$suite" "$name" "$RESET"

    # ---------- output ----------
    "$test_main" "$ifj" > "$actual" 2> "$errors"
    code=$?

    if [ ! -f "$out_file" ]; then
      fail "output" "($name.out is missing)"
      test_ok=false
    elif diff -q "$out_file" "$actual" > /dev/null; then
      pass "output"
    else
      fail "output" "(- expected, + actual)"
      diff -u --label "$name.out" --label "actual output" \
        "$out_file" "$actual" | indent
      test_ok=false
    fi

    # ---------- return code ----------
    expected_code=0
    if [ -f "$code_file" ]; then
      expected_code="$(tr -d '[:space:]' < "$code_file")"
    fi

    if [ "$code" = "$expected_code" ]; then
      pass "return code" "($code)"
    else
      fail "return code" "(expected $expected_code, got $code)"
      if [ -s "$errors" ]; then
        indent < "$errors"
      fi
      test_ok=false
    fi

    # ---------- valgrind ----------
    if [ "$has_valgrind" = true ]; then
      valgrind --quiet --leak-check=full --show-leak-kinds=all \
        --errors-for-leak-kinds=definite,indirect,possible \
        --error-exitcode="$VALGRIND_ERR" \
        --log-file="$tmp_dir/$suite.$name.valgrind" \
        "$test_main" "$ifj" > /dev/null 2>&1
      if [ $? -eq "$VALGRIND_ERR" ]; then
        fail "valgrind"
        indent < "$tmp_dir/$suite.$name.valgrind"
        test_ok=false
      else
        pass "valgrind"
      fi
    else
      skip "valgrind" "(not installed)"
    fi

    if [ "$test_ok" = true ]; then
      passed=$((passed + 1))
    else
      failed=$((failed + 1))
      failed_names+=("$suite/$name")
    fi
  done
}

for suite in "${suites[@]}"; do
  run_suite "$suite"
done

# ---------- summary ----------
printf '\n%s========== summary ==========%s\n' "$BOLD" "$RESET"
printf '  %s%d passed%s, %s%d failed%s\n' \
  "$GREEN" "$passed" "$RESET" "$RED" "$failed" "$RESET"

if [ "$failed" -gt 0 ]; then
  printf '  failed:\n'
  printf '    %s\n' "${failed_names[@]}"
  exit 1
fi
exit 0
