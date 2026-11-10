#!/bin/bash
# запуск тестов: make test
cd "$(dirname "$0")" || exit 1
mkdir -p logs
failed=0
check() {
    if [ "$1" = 0 ]; then echo "[OK]   $2";
    else echo "[FAIL] $2"; failed=1; fi
}

# Независимые программы запускаются теми же командами, что и вручную.
run() {
    local mode=$1 name=$2; shift 2
    local out="logs/${mode}_${name}.out" log="logs/${mode}_${name}.log"
    if [ "$mode" != independent ]; then
        if [ "$name" = unlimited ]; then
            timeout --preserve-status -s INT 1 "./ferry_$mode" "$@" tick_ms=2 log_path="$log" > "$out"
        else
            "./ferry_$mode" "$@" tick_ms=2 log_path="$log" > "$out"
        fi
        return $?
    fi
    : > "$out"
    ./ferry_independent "$@" tick_ms=2 log_path="$log" > "$out" &
    local owner=$!
    until grep -q 'IPC готово' "$out"; do
        if ! kill -0 "$owner" 2>/dev/null; then wait "$owner"; return $?; fi
        sleep 0.01
    done
    ./ferry_process >> "$out" & local ferry=$!
    ./dispatcher_process >> "$out" & local disp=$!
    ./cars_process >> "$out" & local cars=$!
    if [ "$name" = unlimited ]; then (sleep 1; kill -INT "$owner") & fi
    wait "$owner"; local rc=$?
    wait "$ferry" "$disp" "$cars"
    return "$rc"
}

for mode in unnamed named independent; do
    for t in basic lookahead emergency one_side; do
        run "$mode" "$t" -c "tests/$t.cfg"
        rc=$?
        [ "$rc" = 0 ] && grep -q 'Не перевезено: 0 ' "logs/${mode}_${t}.log" &&
            ! grep -q 'НАРУШЕНИЕ' "logs/${mode}_${t}.log"
        check $? "$mode: $t"
    done
    run "$mode" trips_limit -c tests/trips_limit.cfg
    rc=$?
    [ "$rc" = 0 ] && grep -q '^Рейсов: 4 ' "logs/${mode}_trips_limit.log"
    check $? "$mode: trips_limit"
    run "$mode" unlimited -c tests/unlimited.cfg
    rc=$?
    [ "$rc" = 0 ] && grep -q 'прервано пользователем' "logs/${mode}_unlimited.log"
    check $? "$mode: unlimited (Ctrl+C)"
    "./ferry_$mode" -c tests/invalid.cfg > /dev/null 2>&1
    check $(( $? != 2 )) "$mode: invalid"
done
exit "$failed"
