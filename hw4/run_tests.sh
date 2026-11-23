#!/bin/bash
# запуск: make test
set -eu
cd "$(dirname "$0")"
mkdir -p logs
port=${PORT:-18428}
pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait
}
trap cleanup EXIT
await() {
    local i
    for ((i=0; i<300; i++)); do
        if grep -q "$1" "$2" 2>/dev/null; then return; fi
        sleep .02
    done
    echo "Не дождались: $1 ($2)" >&2
    return 1
}
start() {
    local name=$1; shift
    timeout 15 ./server 127.0.0.1 "$port" -c "tests/$name.cfg" "$@" > "logs/$name.server" &
    server=$!; pids=("$server")
    await 'Сервер готов' "logs/$name.server"
}
watch() {
    timeout 12 ./observer 127.0.0.1 "$port" > "$1" &
    watcher=$!; pids+=("$watcher")
    await 'Подключение принято' "$1"
}
shores() {
    timeout 12 ./shore 127.0.0.1 "$port" left > "logs/$1.left" &
    left=$!; pids+=("$left")
    if [ "$1" != one_side ]; then
        timeout 12 ./shore 127.0.0.1 "$port" right > "logs/$1.right" &
        right=$!; pids+=("$right")
    fi
}
for t in basic lookahead emergency one_side trips_limit; do
    start "$t" delay_ms=0
    watch "logs/$t.log"
    shores "$t"
    wait "$server"; wait "$watcher"
    wait "$left"
    if [ "$t" != one_side ]; then wait "$right"; fi
    ! grep -q 'НАРУШЕНИЕ' "logs/$t.log" || exit 1
    if [ "$t" = trips_limit ]; then
        grep -q 'выполнено заданное число рейсов' "logs/$t.log"
        grep -q 'Не перевезено: [1-9]' "logs/$t.log"
    else
        grep -q 'Не перевезено: 0 ' "logs/$t.log"
    fi
    if [ "$t" = basic ]; then
        grep -q 'Приехала машина.*левый берег' logs/basic.left
        ! grep -q 'Приехала машина.*правый берег' logs/basic.left || exit 1
        grep -q 'Приехала машина.*правый берег' logs/basic.right
        ! grep -q 'Итоги:' logs/basic.server || exit 1
    fi
    pids=()
    echo "[OK] $t"
done

start unlimited delay_ms=8
watch logs/unlimited.log
first=$watcher
watch logs/second.log
shores unlimited
await 'Отправление' logs/second.log
kill "$watcher"
wait "$watcher" || true
watch logs/reconnected.log
await 'Состояние:' logs/reconnected.log
await 'Паром приплыл' logs/reconnected.log
kill -INT "$server"
wait "$server"; wait "$first"; wait "$watcher"; wait "$left"; wait "$right"
grep -q 'прервано пользователем' logs/unlimited.log
grep -q 'прервано пользователем' logs/reconnected.log
pids=()
echo '[OK] два наблюдателя, повторное подключение, Ctrl+C'

rc=0
./server 127.0.0.1 "$port" -c tests/invalid.cfg > /dev/null 2>&1 || rc=$?
[ "$rc" = 2 ]
echo '[OK] invalid'
