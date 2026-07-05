PASSED=0
FAILED=0
TOTAL=0

echo "Running MiniLang Test Suite"
echo "==========================="
echo ""

for test_file in tests/programs/[0-9]*.ml; do
    test_name=$(basename "$test_file" .ml)
    expected_file="tests/programs/${test_name}.expected"
    
    if [ ! -f "$expected_file" ]; then
        echo "SKIP $test_name"
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    input_file="tests/programs/${test_name}.input"
    if [ -f "$input_file" ]; then
        actual=$(./minilang run "$test_file" < "$input_file" 2>/dev/null)
    else
        actual=$(./minilang run "$test_file" 2>/dev/null)
    fi
    expected=$(cat "$expected_file")
    
    if [ "$actual" = "$expected" ]; then
        echo "PASS $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL $test_name"
        echo "  Expected:"
        echo "$expected"
        echo "  Actual:"
        echo "$actual"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "==========================="
echo "Results: $PASSED passed, $FAILED failed, $TOTAL total"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
