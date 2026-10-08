#!/bin/bash
# запуск тестов: сначала make, потом ./run_tests.sh
cd "$(dirname "$0")"; mkdir -p logs
check() { [ "$1" = 0 ] && echo "[OK]   $2" || echo "[FAIL] $2"; }

for t in basic lookahead emergency one_side; do
    ./ferry -c tests/$t.cfg delay_ms=0 log_path=logs/$t.log > logs/$t.out
    grep -q "все автомобили перевезены" logs/$t.out && ! grep -q "НАРУШЕНИЕ" logs/$t.out
    check $? $t
done

./ferry -c tests/trips_limit.cfg delay_ms=0 log_path=logs/trips_limit.log > logs/trips_limit.out
grep -q "выполнено заданное число рейсов" logs/trips_limit.out && grep -q "Не перевезено: [1-9]" logs/trips_limit.out && ! grep -q "НАРУШЕНИЕ" logs/trips_limit.out
check $? trips_limit

./ferry -c tests/invalid.cfg > /dev/null 2>&1
check $(( $? != 2 )) "invalid (должна быть ошибка)"

timeout --preserve-status -s INT 2 ./ferry -c tests/unlimited.cfg delay_ms=1 log_path=logs/unlimited.log > /dev/null
check $? "unlimited (остановка по Ctrl+C)"
