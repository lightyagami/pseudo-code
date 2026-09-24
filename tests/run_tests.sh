#!/bin/bash
set -e

PSEUDOC="./pseudoc"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR" test_sample.txt' EXIT

FAILED=0
PASSED=0

run_test() {
    local file="$1"
    local stdin_input="$2"
    local name=$(basename "$file")

    local vm_out="$TMP_DIR/${name}.vm.out"
    local c_src="$TMP_DIR/${name}.c"
    local c_bin="$TMP_DIR/${name}.bin"
    local c_out="$TMP_DIR/${name}.c.out"

    # Run on VM
    if [ -n "$stdin_input" ]; then
        echo -e "$stdin_input" | $PSEUDOC "$file" > "$vm_out"
    else
        $PSEUDOC "$file" > "$vm_out"
    fi

    # Compile to C and run
    $PSEUDOC "$file" -o "$c_src"
    gcc -O2 "$c_src" -o "$c_bin" -lm
    if [ -n "$stdin_input" ]; then
        echo -e "$stdin_input" | "$c_bin" > "$c_out"
    else
        "$c_bin" > "$c_out"
    fi

    # Differential assertion
    if diff -u "$vm_out" "$c_out" > "$TMP_DIR/${name}.diff"; then
        echo "  [PASS] $file (VM == C Backend)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $file: VM and C outputs differ!"
        cat "$TMP_DIR/${name}.diff"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Pseudoc Differential Test Suite ==="

run_test "tour.pseudo" ""
run_test "fizzbuzz.pseudo" ""
run_test "sum.pseudo" "10"
run_test "tests/test_procedures.pseudo" ""
run_test "tests/test_records.pseudo" ""
run_test "tests/test_case.pseudo" ""
run_test "tests/test_file_io.pseudo" ""
run_test "tests/test_repeat.pseudo" ""
run_test "tests/test_constants.pseudo" ""
run_test "tests/test_builtins.pseudo" ""
run_test "tests/test_oop.pseudo" ""
run_test "tests/test_banking_oop.pseudo" ""

run_c_to_pseudo_test() {
    local file="$1"
    local name=$(basename "$file")

    local c_bin="$TMP_DIR/${name}.c.bin"
    local c_out="$TMP_DIR/${name}.c.out"
    local pseudo_src="$TMP_DIR/${name}.pseudo"
    local pseudo_out="$TMP_DIR/${name}.pseudo.out"

    # Compile and run original C file
    gcc -O2 "$file" -o "$c_bin" -lm
    "$c_bin" > "$c_out"

    # Transpile C to pseudocode with pseudoc
    $PSEUDOC "$file" -o "$pseudo_src"

    # Run the reverse-transpiled pseudocode on Pseudoc VM
    $PSEUDOC "$pseudo_src" > "$pseudo_out"

    # Assert exact differential match
    if diff -u "$c_out" "$pseudo_out" > "$TMP_DIR/${name}.diff"; then
        echo "  [PASS] $file (C -> Pseudocode -> VM == Native C)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $file: Native C and Reverse Pseudocode outputs differ!"
        cat "$TMP_DIR/${name}.diff"
        FAILED=$((FAILED + 1))
    fi
}

run_c_to_pseudo_test "tests/test_c_to_pseudo.c"

echo "------------------------------------------------"
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
