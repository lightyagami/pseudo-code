#!/bin/bash
set -e

PSEUDOC="./pseudoc"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR" test_sample.txt test_books.dat' EXIT

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
run_test "tests/test_composite_params.pseudo" ""
run_test "tests/test_random_files.pseudo" ""
run_test "tests/test_array_return.pseudo" ""

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
    run_py_test "tests/test_composite_params.pseudo" ""
    run_py_test "tests/test_random_files.pseudo" ""
    run_py_test "tests/test_array_return.pseudo" ""
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
for check_f in "tour.pseudo" "tests/test_banking_oop.pseudo" "tests/test_composite_params.pseudo" "tests/test_c_to_pseudo.c"; do
    if $PSEUDOC --check "$check_f"; then
        echo "  [PASS] $check_f (--check passed)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $check_f (--check failed)"
        FAILED=$((FAILED + 1))
    fi
done

echo "=== Running Code Formatter Tests (--format) ==="
formatted_out=$($PSEUDOC --format - << 'EOF'
declare x:integer
if x=1 then
output "one"
else
output "other"
endif
EOF
)
if echo "$formatted_out" | grep -q "DECLARE x : INTEGER" && echo "$formatted_out" | grep -q "    OUTPUT \"one\""; then
    echo "  [PASS] Code formatter normalizes casing and indentation"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Code formatter produced unexpected output:"
    echo "$formatted_out"
    FAILED=$((FAILED + 1))
fi

echo "=== Running Negative --check Tests (Expected Failures) ==="
run_negative_check() {
    local desc="$1"
    local code="$2"
    local bad_file="$TMP_DIR/bad_$$.pseudo"
    echo -e "$code" > "$bad_file"
    if $PSEUDOC --check "$bad_file" >/dev/null 2>&1; then
        echo "  [FAIL] $desc: expected --check failure, but it succeeded!"
        FAILED=$((FAILED + 1))
    else
        echo "  [PASS] $desc (properly rejected)"
        PASSED=$((PASSED + 1))
    fi
}

run_negative_check "Type mismatch assignment" "DECLARE x : INTEGER\nx <- \"hello\""
run_negative_check "Undeclared variable" "undeclared_var <- 42"
run_negative_check "Arity mismatch" "FUNCTION f(a : INTEGER) RETURNS INTEGER\nRETURN a\nENDFUNCTION\nDECLARE res : INTEGER\nres <- f(1, 2)"
run_negative_check "Duplicate declaration in same scope" "DECLARE x : INTEGER\nDECLARE x : INTEGER"

echo "=== Running Conformance & Torture Differential Tests ==="
for t_file in \
    "tests/torture/t01_bounds.pseudo" \
    "tests/torture/t02_divmod.pseudo" \
    "tests/torture/t06_byref.pseudo" \
    "tests/torture/t07_byref_record.pseudo" \
    "tests/torture/t08_byval_no_mutate.pseudo" \
    "tests/torture/t09_fibonacci.pseudo" \
    "tests/torture/t10_2d_array.pseudo" \
    "tests/torture/t11_string_concat.pseudo" \
    "tests/torture/t12_string_funcs.pseudo" \
    "tests/torture/t13_real_arith.pseudo" \
    "tests/torture/t14_boolean.pseudo" \
    "tests/torture/t15_nested_if.pseudo" \
    "tests/torture/t16_case.pseudo" \
    "tests/torture/t17_sort_ties.pseudo" \
    "tests/torture/t18_file_io.pseudo" \
    "tests/torture/t19_eof.pseudo" \
    "tests/torture/t20_random_file.pseudo"; do
    run_test "$t_file" ""
    if command -v python3 >/dev/null 2>&1; then
        run_py_test "$t_file" ""
    fi
done

run_runtime_error_test() {
    local desc="$1"
    local file="$2"
    local vm_ok=0 c_ok=0 py_ok=0

    # VM should exit non-zero
    if ! $PSEUDOC "$file" >/dev/null 2>&1; then vm_ok=1; fi

    # C should exit non-zero
    local c_src="$TMP_DIR/err.c"
    local c_bin="$TMP_DIR/err.bin"
    $PSEUDOC "$file" -o "$c_src"
    gcc -O2 "$c_src" -o "$c_bin" -lm >/dev/null 2>&1
    if ! "$c_bin" >/dev/null 2>&1; then c_ok=1; fi

    # Python should exit non-zero
    local py_src="$TMP_DIR/err.py"
    $PSEUDOC "$file" -o "$py_src"
    if ! python3 "$py_src" >/dev/null 2>&1; then py_ok=1; fi

    if [ $vm_ok -eq 1 ] && [ $c_ok -eq 1 ] && [ $py_ok -eq 1 ]; then
        echo "  [PASS] $desc (VM, C, and Python all properly error)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $desc: expected error did not trigger on all backends (VM:$vm_ok, C:$c_ok, Py:$py_ok)"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Runtime Fault & Overflow Verification (VM, C, Python) ==="
run_runtime_error_test "Array out-of-bounds access" "tests/torture/t01b_oob.pseudo"
run_runtime_error_test "64-bit integer addition overflow" "tests/torture/t03_overflow_add.pseudo"
run_runtime_error_test "64-bit integer multiplication overflow" "tests/torture/t04_overflow_mul.pseudo"
run_runtime_error_test "Integer division by zero" "tests/torture/t05_divzero.pseudo"

if command -v node >/dev/null 2>&1 && [ -f "web/pseudoc.js" ]; then
    echo "=== Running WebAssembly / Node.js Engine Verification ==="
    if node -e "
const createPseudocModule = require('./web/pseudoc.js');
createPseudocModule().then(Module => {
    const run_vm = Module.cwrap('wasm_run_vm', 'string', ['string', 'string']);
    const check = Module.cwrap('wasm_check', 'string', ['string']);
    const emit_c = Module.cwrap('wasm_emit_c', 'string', ['string']);
    const emit_py = Module.cwrap('wasm_emit_py', 'string', ['string']);
    const dump_bc = Module.cwrap('wasm_dump_bytecode', 'string', ['string']);
    const format = Module.cwrap('wasm_format', 'string', ['string']);

    const code = 'DECLARE x : INTEGER\nx <- 42\nOUTPUT \"WASM_OK: \", x';
    const out = run_vm(code, '');
    if (!out.includes('WASM_OK: 42')) process.exit(1);

    const chk = check(code);
    if (!chk.includes('OK')) process.exit(1);

    const c = emit_c(code);
    if (!c.includes('int main')) process.exit(1);

    const py = emit_py(code);
    if (!py.includes('x = 42')) process.exit(1);

    const bc = dump_bc(code);
    if (!bc.includes('OP_HALT')) process.exit(1);

    const fmt = format('declare y:integer\noutput y');
    if (!fmt.includes('DECLARE y : INTEGER')) process.exit(1);

    process.exit(0);
}).catch(() => process.exit(1));
" >/dev/null 2>&1; then
        echo "  [PASS] WebAssembly pseudoc engine (VM, C, Python, Bytecode, Check, Formatter)"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] WebAssembly engine failed in Node.js runtime!"
        FAILED=$((FAILED + 1))
    fi
fi

echo "------------------------------------------------"
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
