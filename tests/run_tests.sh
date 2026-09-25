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

    # Differential assertion (VM == C)
    if diff -u "$vm_out" "$c_out" > "$TMP_DIR/${name}.diff"; then
        echo "  [PASS] $file (VM == C Backend)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $file: VM and C outputs differ!"
        cat "$TMP_DIR/${name}.diff"
        FAILED=$((FAILED + 1))
    fi
}

run_py_test() {
    local file="$1"
    local stdin_input="$2"
    local name=$(basename "$file")

    local vm_out="$TMP_DIR/${name}.vm.out"
    local py_src="$TMP_DIR/${name}.py"
    local py_out="$TMP_DIR/${name}.py.out"

    # Run on VM first if not already run
    if [ ! -f "$vm_out" ]; then
        if [ -n "$stdin_input" ]; then
            echo -e "$stdin_input" | $PSEUDOC "$file" > "$vm_out"
        else
            $PSEUDOC "$file" > "$vm_out"
        fi
    fi

    # Transpile to Python and run
    $PSEUDOC "$file" -o "$py_src"
    if [ -n "$stdin_input" ]; then
        echo -e "$stdin_input" | python3 "$py_src" > "$py_out"
    else
        python3 "$py_src" > "$py_out"
    fi

    # Differential assertion (VM == Python)
    if diff -u "$vm_out" "$py_out" > "$TMP_DIR/${name}.py.diff"; then
        echo "  [PASS] $file (VM == Python Transpiler)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $file: VM and Python outputs differ!"
        cat "$TMP_DIR/${name}.py.diff"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Pseudoc Differential Test Suite (VM == C Backend) ==="

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

if command -v python3 >/dev/null 2>&1; then
    echo "=== Running Python 3 Transpiler Tests (VM == Python) ==="
    run_py_test "tour.pseudo" ""
    run_py_test "fizzbuzz.pseudo" ""
    run_py_test "sum.pseudo" "10"
    run_py_test "tests/test_case.pseudo" ""
    run_py_test "tests/test_repeat.pseudo" ""
    run_py_test "tests/test_constants.pseudo" ""
    run_py_test "tests/test_builtins.pseudo" ""
    run_py_test "tests/test_oop.pseudo" ""
    run_py_test "tests/test_banking_oop.pseudo" ""
fi

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

echo "=== Running Reverse C-to-Pseudocode Tests ==="
run_c_to_pseudo_test "tests/test_c_to_pseudo.c"

echo "=== Running --check / Syntax & Sema Only Tests ==="
for check_f in "tour.pseudo" "tests/test_banking_oop.pseudo" "tests/test_c_to_pseudo.c"; do
    if $PSEUDOC --check "$check_f"; then
        echo "  [PASS] $check_f (--check passed)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $check_f (--check failed)"
        FAILED=$((FAILED + 1))
    fi
done

echo "------------------------------------------------"
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
