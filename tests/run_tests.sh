#!/usr/bin/env bash
#
# @file run_tests.sh
# @brief Runs the test suites from tests/<suite>/
# @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
# @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
# @author Oldřich Vašíček <xvasico00@stud.fit.vut.cz>
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
# Disclaimer!
# The codegen suite was added by Claude Opus 5.5 AI model
# Prompt:
#  "Ok, rozsir tedy prosim testy o codegen testy, aby mely stejnou architekturu
#   jak testy pred tim, stejny prompt atd. Jestli budou testy potrebovat
#   interpret IFJ26code, udelej je tak, aby interpret ic26int-linux-x86_64
#   testy ocekavaly v root slozce repa, tedy ve stejne slozce jako je slozka
#   src."
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
# The codegen suite differs in step 1 and 2: tests/codegen_main prints the
# generated IFJcode26 program, which is then run by the interpreter
# ic26int-linux-x86_64 from the repository root with NAME.in on its stdin
# (empty stdin if there's no NAME.in). The program's output is compared with
# NAME.out and NAME.code holds the interpreter's return code, or the
# compiler's one if the compilation failed (then the output must be empty).
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

# the IFJcode26 interpreter for the codegen suite, it isn't versioned,
# everyone downloads it from the e-learning into the repository root
IC26INT="$ROOT_DIR/ic26int-linux-x86_64"

# max seconds of one interpretation, so an endless loop in the generated
# code fails the test instead of blocking the whole run (code 124)
INTERPRET_TIMEOUT=10

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

# compiles one program of the codegen suite and interprets the result
# params: testing main, NAME.ifj, NAME.in, file for the generated code,
#         file for the program's output, file for the error output
# returns the compiler's code if it failed, else the interpreter's code
run_codegen() {
  local test_main="$1" ifj="$2" in_file="$3" code26="$4" actual="$5"
  local errors="$6" compile_code

  "$test_main" "$ifj" > "$code26" 2> "$errors"
  compile_code=$?
  if [ "$compile_code" -ne 0 ]; then
    # there's nothing to interpret, the program printed nothing
    : > "$actual"
    return "$compile_code"
  fi

  if [ ! -f "$in_file" ]; then
    in_file=/dev/null
  fi
  timeout "$INTERPRET_TIMEOUT" "$IC26INT" "$code26" < "$in_file" \
    > "$actual" 2>> "$errors"
}

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

  if [ "$suite" = codegen ] && [ ! -x "$IC26INT" ]; then
    printf '%sInterpreter %s not found or not executable, download it from\n' \
      "$RED" "$IC26INT"
    printf 'the e-learning into the repository root and run chmod +x on it.%s\n' \
      "$RESET"
    failed=$((failed + 1))
    failed_names+=("$suite/(no interpreter)")
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
    if [ "$suite" = codegen ]; then
      run_codegen "$test_main" "$ifj" "$test_dir/$name.in" \
        "$tmp_dir/$suite.$name.code26" "$actual" "$errors"
    else
      "$test_main" "$ifj" > "$actual" 2> "$errors"
    fi
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
