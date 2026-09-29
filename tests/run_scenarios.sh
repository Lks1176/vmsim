#!/bin/sh
# Pruebas de escenarios: ejecuta el simulador sobre cargas de trabajo y compara
# las estadísticas con valores calculados a mano (ver REPORTE.md).
cd "$(dirname "$0")/.." || exit 1
BIN=build/vmsim
W=tests/workloads
fail=0

# expect <descripción> <valor esperado> <valor obtenido>
expect() {
    if [ "$2" = "$3" ]; then echo "  ok   $1 = $3"
    else echo "  FAIL $1: esperado '$2', obtenido '$3'"; fail=1; fi
}
# stat <salida> <etiqueta> : valor numérico tras "etiqueta: "
stat() { echo "$1" | sed -n "s/^$2: \([0-9.]*\).*/\1/p" | head -1; }

# verify_reads <archivo> <salida>: cada `read` debe devolver el último valor
# escrito en esa dirección (0 si nunca se escribió). Solo cargas sin free.
verify_reads() {
    expected=$(sed 's/#.*//' "$1" | tr -s ' \t\r\n' '\n\n\n\n' | awk 'NF' | awk '
        { t[NR] = $0 }
        END { i = 1
              while (i <= NR) {
                  c = t[i]
                  if (c == "alloc" || c == "free") i += 2
                  else if (c == "write") { mem[t[i+1]+0] = t[i+2]+0; i += 3 }
                  else if (c == "read") { a = t[i+1]+0; print ((a in mem) ? mem[a] : 0); i += 2 }
                  else i++ } }')
    obtained=$(echo "$2" | awk '/^read /{print $NF}')
    if [ "$expected" = "$obtained" ]; then echo "  ok   integridad de datos en todas las lecturas"
    else echo "  FAIL integridad de datos (el swap devolvió valores incorrectos)"; fail=1; fi
}

echo "[01] ejemplo del enunciado"
out=$($BIN $W/01_basico.txt)
expect "accesos" 4 "$(stat "$out" 'Total de accesos')"
expect "fallos" 2 "$(stat "$out" 'Total fallos de página')"
expect "hit rate" 50.00 "$(stat "$out" 'Hit rate')"
expect "reemplazos" 0 "$(stat "$out" 'Total reemplazos')"
verify_reads $W/01_basico.txt "$out"

echo "[02] recorrido cíclico (peor caso de FIFO)"
out=$($BIN $W/02_secuencial_ciclico.txt)
expect "accesos" 384 "$(stat "$out" 'Total de accesos')"
expect "fallos" 384 "$(stat "$out" 'Total fallos de página')"
expect "hit rate" 0.00 "$(stat "$out" 'Hit rate')"
expect "reemplazos" 320 "$(stat "$out" 'Total reemplazos')"
verify_reads $W/02_secuencial_ciclico.txt "$out"

echo "[03] localidad de referencia"
out=$($BIN $W/03_localidad.txt)
expect "accesos" 5000 "$(stat "$out" 'Total de accesos')"
verify_reads $W/03_localidad.txt "$out"
hr=$(stat "$out" 'Hit rate')
if [ "$(echo "$hr" | awk '{print ($1 > 85) ? 1 : 0}')" = 1 ]; then echo "  ok   hit rate $hr% > 85%"
else echo "  FAIL hit rate $hr% <= 85%"; fail=1; fi

echo "[04] anomalía de Belady (FIFO: más marcos, MÁS fallos)"
f4=$(stat "$($BIN -q -s 65536 -m 256 $W/04_belady.txt)" 'Total fallos de página')
f5=$(stat "$($BIN -q -s 65536 -m 320 $W/04_belady.txt)" 'Total fallos de página')
expect "fallos con 4 marcos" 14 "$f4"
expect "fallos con 5 marcos" 16 "$f5"

echo "[05] manejo de errores (recuperables) y ciclo de vida"
out=$($BIN $W/05_errores.txt 2>/dev/null); code=$?
expect "código de salida" 0 "$code"
expect "comandos con error" 6 "$(echo "$out" | sed -n 's/^Comandos con error: \([0-9]*\).*/\1/p')"
expect "free realizados" "3 / 1" "$(echo "$out" | sed -n 's/^Reservas (alloc) \/ liberaciones (free): \(.*\)$/\1/p' | sed 's/^\([0-9]*\) \/ \([0-9]*\)$/\1 \/ \2/')"

echo "[06] errores de uso y de sintaxis"
$BIN -m 100 $W/01_basico.txt >/dev/null 2>&1; expect "RAM < 256 KB -> salida" 1 $?
$BIN -p lru $W/01_basico.txt >/dev/null 2>&1; expect "política no soportada -> salida" 1 $?
$BIN /no/existe.txt >/dev/null 2>&1; expect "archivo inexistente -> salida" 2 $?
echo "alloc 4096 jump 3" | $BIN -q >/dev/null 2>&1; expect "sintaxis inválida -> salida" 2 $?

if [ $fail -eq 0 ]; then echo "TODAS LAS PRUEBAS DE ESCENARIOS PASARON"; else echo "HAY PRUEBAS FALLIDAS"; fi
exit $fail
