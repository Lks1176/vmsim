#!/bin/sh
# Barrido de tamaño de memoria física sobre una carga: imprime una tabla
# fallos / hit rate / reemplazos. Uso: tests/sweep.sh <carga> [KB ...]
cd "$(dirname "$0")/.." || exit 1
FILE=${1:?uso: sweep.sh <carga> [KB ...]}; shift
[ $# -eq 0 ] && set -- 256 320 384 512 768 1024
printf "%-8s %-8s %-10s %-10s %s\n" "RAM(KB)" "marcos" "fallos" "hit rate" "reemplazos"
for kb in "$@"; do
    out=$(./build/vmsim -q -m "$kb" "$FILE")
    f=$(echo "$out" | sed -n 's/^Total fallos de página: //p')
    h=$(echo "$out" | sed -n 's/^Hit rate: //p')
    r=$(echo "$out" | sed -n 's/^Total reemplazos: //p')
    printf "%-8s %-8s %-10s %-10s %s\n" "$kb" "$((kb / 4))" "$f" "$h" "$r"
done
