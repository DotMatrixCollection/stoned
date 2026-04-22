#!/usr/bin/env bash
set -u

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT_DIR/stoned"
CASES_DIR="$ROOT_DIR/tests/cases"

if [ ! -x "$BIN" ]; then
    echo "missing interpreter binary: $BIN" >&2
    echo "run 'make' first" >&2
    exit 1
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

total=0
passed=0
failed=0

read_expected_status() {
    local status_file=$1
    if [ -f "$status_file" ]; then
        tr -d ' \n\r\t' < "$status_file"
    else
        printf '0'
    fi
}

compare_file() {
    local expected=$1
    local actual=$2
    local label=$3

    if cmp -s "$expected" "$actual"; then
        return 0
    fi

    echo "    $label mismatch"
    echo "    expected:"
    sed 's/^/      /' "$expected"
    echo "    actual:"
    sed 's/^/      /' "$actual"
    return 1
}

run_case() {
    local stem=$1
    local input_file=""
    local mode=""
    local actual_out="$tmpdir/$stem.out"
    local actual_err="$tmpdir/$stem.err"
    local expected_out="$CASES_DIR/$stem.out"
    local expected_err="$CASES_DIR/$stem.err"
    local compare_out="$tmpdir/$stem.expected.out"
    local compare_err="$tmpdir/$stem.expected.err"
    local expected_status
    local status
    local ok=1

    if [ -f "$CASES_DIR/$stem.rb" ]; then
        input_file="$CASES_DIR/$stem.rb"
        mode="file"
    elif [ -f "$CASES_DIR/$stem.stdin" ]; then
        input_file="$CASES_DIR/$stem.stdin"
        mode="stdin"
    else
        echo "not ok - $stem"
        echo "    missing input fixture"
        failed=$((failed + 1))
        total=$((total + 1))
        return
    fi

    expected_status=$(read_expected_status "$CASES_DIR/$stem.status")

    if [ "$mode" = "file" ]; then
        "$BIN" "$input_file" >"$actual_out" 2>"$actual_err"
        status=$?
    else
        "$BIN" <"$input_file" >"$actual_out" 2>"$actual_err"
        status=$?
    fi

    total=$((total + 1))

    if [ "$status" != "$expected_status" ]; then
        echo "not ok - $stem"
        echo "    exit status mismatch: expected $expected_status, got $status"
        failed=$((failed + 1))
        return
    fi

    if [ -f "$expected_out" ]; then
        cp "$expected_out" "$compare_out"
    else
        : >"$compare_out"
    fi
    if [ -f "$expected_err" ]; then
        cp "$expected_err" "$compare_err"
    else
        : >"$compare_err"
    fi

    if ! compare_file "$compare_out" "$actual_out" "stdout"; then
        ok=0
    fi
    if ! compare_file "$compare_err" "$actual_err" "stderr"; then
        ok=0
    fi

    if [ "$ok" -eq 1 ]; then
        echo "ok - $stem"
        passed=$((passed + 1))
    else
        echo "not ok - $stem"
        failed=$((failed + 1))
    fi
}

while IFS= read -r path; do
    file=${path##*/}
    stem=${file%.*}
    run_case "$stem"
done < <(find "$CASES_DIR" -maxdepth 1 \( -name '*.rb' -o -name '*.stdin' \) | sort)

echo
echo "$passed passed, $failed failed, $total total"

if [ "$failed" -ne 0 ]; then
    exit 1
fi
