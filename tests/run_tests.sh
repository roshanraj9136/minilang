#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MINILANG="$SCRIPT_DIR/../minilang"
PASS=0
FAIL=0
ERRORS=""

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [ ! -x "$MINILANG" ]; then
    echo -e "${RED}Error: minilang binary not found or not executable at $MINILANG${NC}"
    exit 1
fi

echo ""
echo "========================================"
echo "  MiniLang Test Suite"
echo "========================================"
echo ""

for test_file in "$SCRIPT_DIR"/*.ml; do
    test_name=$(basename "$test_file" .ml)
    expected_file="$SCRIPT_DIR/${test_name}.expected"

    if [ ! -f "$expected_file" ]; then
        echo -e "  ${YELLOW}SKIP${NC}  $test_name  (no .expected file)"
        continue
    fi

    actual=$("$MINILANG" run "$test_file" 2>&1)
    exit_code=$?
    expected=$(cat "$expected_file")

    if [ "$actual" = "$expected" ] && [ $exit_code -eq 0 ]; then
        echo -e "  ${GREEN}PASS${NC}  $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $test_name"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n--- $test_name ---\n"
        if [ $exit_code -ne 0 ]; then
            ERRORS="${ERRORS}Exit code: $exit_code\n"
        fi
        ERRORS="${ERRORS}Expected:\n${expected}\nActual:\n${actual}\n"
    fi
done

TOTAL=$((PASS + FAIL))
echo ""
echo "========================================"
echo -e "  Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC} / ${TOTAL} total"
echo "========================================"

if [ $FAIL -gt 0 ]; then
    echo ""
    echo "Failures:"
    echo -e "$ERRORS"
    exit 1
fi

echo ""
exit 0
