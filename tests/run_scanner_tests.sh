# @file run_scanner_tests.sh
# @brief Testing script for the scanner
# @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
# @date 16.10.2026



# Disclaimer!
# This testing file was made by Claude Opus 5.5 AI model in xHigh effort mode
# This file is only for testing purposes.
# Prompt:
#  Create a testing bash script for the scanner that makes the project, runs scanner_main with one
#   .ifj at a time as argv[1], compares the output with respective .out file and checks the return code
#   and does so with each .ifj and .out file which the script separates into sections. The script's
#   output should be whether the test ran correctly and in case it didnt then the script should show
#   the differences. The script will also run valgrind for memory leaks check.


#!/usr/bin/env bash
#
# @file run_scanner_tests.sh
# @brief Runs all scanner tests from tests/scanner/
# @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
# @date 16.10.2026
#
# Builds the project, then for every NAME.ifj in tests/scanner/:
#   1. runs tests/scanner_main with NAME.ifj as argv[1]
#   2. compares its output with NAME.out
#   3. compares its return code with NAME.code (expects 0 if there's none)
#   4. runs it again under valgrind to find memory errors and leaks
# Every test gets its own section with PASS/FAIL for each part,
# the differences are shown in case of a failure.
#
# Usage: tests/run_scanner_tests.sh
#
# Return: 0 if every test passed
#         1 if at least one test failed
#         2 if the project couldn't be built

set -u
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$SCRIPT_DIR/scanner"
SCANNER_MAIN="$SCRIPT_DIR/scanner_main"

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

# ---------- build ----------
printf '%sBuilding the project and scanner_main...%s\n' "$BOLD" "$RESET"
if ! make -s -C "$ROOT_DIR" all scanner_test; then
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
tests=("$TEST_DIR"/*.ifj)
if [ ${#tests[@]} -eq 0 ]; then
  printf '%sNo .ifj files found in %s%s\n' "$RED" "$TEST_DIR" "$RESET"
  exit 1
fi

passed=0
failed=0
failed_names=()

for ifj in "${tests[@]}"; do
  name="$(basename "$ifj" .ifj)"
  out_file="$TEST_DIR/$name.out"
  code_file="$TEST_DIR/$name.code"
  actual="$tmp_dir/$name.actual"
  errors="$tmp_dir/$name.stderr"
  test_ok=true

  printf '\n%s========== %s ==========%s\n' "$BOLD" "$name" "$RESET"

  # ---------- output ----------
  "$SCANNER_MAIN" "$ifj" > "$actual" 2> "$errors"
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
      --log-file="$tmp_dir/$name.valgrind" \
      "$SCANNER_MAIN" "$ifj" > /dev/null 2>&1
    if [ $? -eq "$VALGRIND_ERR" ]; then
      fail "valgrind"
      indent < "$tmp_dir/$name.valgrind"
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
    failed_names+=("$name")
  fi
done

# ---------- summary ----------
printf '\n%s========== summary ==========%s\n' "$BOLD" "$RESET"
printf '  %s%d passed%s, %s%d failed%s\n' \
  "$GREEN" "$passed" "$RESET" "$RED" "$failed" "$RESET"

if [ "$failed" -gt 0 ]; then
  printf '  failed: %s\n' "${failed_names[*]}"
  exit 1
fi
exit 0
