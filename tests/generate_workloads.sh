#!/bin/sh
# Genera las cargas de trabajo reproducibles en tests/workloads/.
# Usa un generador congruencial fijo (16807 mod 2^31-1), por lo que la salida
# es idéntica en cualquier máquina.
set -e
OUT="$(dirname "$0")/workloads"
mkdir -p "$OUT"

# 02: recorrido cíclico de 96 páginas (384 KB) con solo 64 marcos (256 KB).
# Es el PEOR CASO de FIFO/LRU: cada acceso de las 3 pasadas de lectura falla.
awk 'BEGIN {
  print "# 96 páginas (384 KB) > 64 marcos. Se escribe una vez y se lee 3 veces en orden.";
  print "alloc 393216";
  for (i = 0; i < 96; i++) print "write " i*4096 " " (i % 256);
  for (p = 0; p < 3; p++) for (i = 0; i < 96; i++) print "read " i*4096;
}' > "$OUT/02_secuencial_ciclico.txt"

# 03: localidad. 90 % de los accesos van a 16 páginas "calientes"; 10 % a
# cualquiera de las 256 páginas (1 MB). 5000 accesos, 30 % escrituras.
awk 'BEGIN {
  s = 12345;
  print "# Localidad de referencia: conjunto caliente de 16 páginas dentro de 256.";
  print "alloc 1048576";
  for (i = 0; i < 5000; i++) {
    s = (s * 16807) % 2147483647; r = s % 100;
    s = (s * 16807) % 2147483647;
    if (r < 90) page = s % 16; else page = s % 256;
    s = (s * 16807) % 2147483647; off = s % 4096;
    s = (s * 16807) % 2147483647;
    if (s % 100 < 30) print "write " page*4096+off " " (s % 256);
    else print "read " page*4096+off;
  }
}' > "$OUT/03_localidad.txt"

# 04: cadena que provoca la ANOMALÍA DE BELADY con FIFO.
# Se ejecuta con páginas de 64 KB: -s 65536 -m 256 (4 marcos) y -m 320 (5 marcos).
awk 'BEGIN {
  n = split("0 3 5 2 2 7 0 0 1 1 1 2 5 4 1 0 3 5 2 7 4 1 3 5", a, " ");
  print "# Anomalía de Belady. Ejecutar con: -s 65536 -m 256  y  -s 65536 -m 320";
  print "alloc 524288";
  for (i = 1; i <= n; i++) print "read " a[i] * 65536;
}' > "$OUT/04_belady.txt"
echo "Cargas generadas en $OUT"
