#!/bin/sh
# Prueba de estrés: 200 000 accesos aleatorios sobre 4096 páginas (16 MB) con
# solo 64 marcos, y verificación de integridad de datos.
cd "$(dirname "$0")/.." || exit 1
mkdir -p build
FILE=build/stress.txt
awk 'BEGIN {
  s = 2024; print "alloc 16777216";
  for (i = 0; i < 200000; i++) {
    s = (s * 16807) % 2147483647; page = s % 4096;
    s = (s * 16807) % 2147483647; addr = page * 4096 + (s % 4096);
    s = (s * 16807) % 2147483647;
    if (s % 2) print "write " addr " " (s % 256); else print "read " addr;
  }
}' > $FILE
echo "Ejecutando $(wc -l < $FILE) comandos..."
out=$(./build/vmsim $FILE)
echo "$out" | grep -E "Total|Hit|Tiempo de CPU"
expected=$(sed 's/#.*//' $FILE | awk '
    $1 == "write" { mem[$2+0] = $3+0 }
    $1 == "read"  { a = $2+0; print ((a in mem) ? mem[a] : 0) }')
obtained=$(echo "$out" | awk '/^read /{print $NF}')
if [ "$expected" = "$obtained" ]; then echo "Integridad de datos: OK"; else echo "Integridad de datos: FALLO"; exit 1; fi
