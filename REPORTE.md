# Reporte de análisis — Simulador de memoria virtual (FIFO)

Política asignada: **FIFO**. Configuración base de todas las pruebas: página de 4 KB,
RAM de 256 KB (64 marcos), salvo donde se indique. Todos los números de este reporte
se obtuvieron ejecutando el simulador (`make test`, `tests/sweep.sh`); los de LRU
provienen de un modelo de referencia aparte (ver §5) y no del simulador.

---

## 1. Descripción de las estructuras de datos

### 1.1 Dirección virtual y tabla de páginas de 2 niveles

```
 31            22 21            12 11             0
 +---------------+---------------+----------------+
 |  PT1 (10 b)   |  PT2 (10 b)   |  Offset (12 b) |
 +---------------+---------------+----------------+
```

- `AddressLayout` (`core/address.c`) calcula la partición. Con página de 4 KB es 10/10/12;
  si el tamaño de página cambia, el offset usa `log2(page)` bits y el resto se reparte
  entre PT2 y PT1.
- `PageTable` (`memory/page_table.c`): un **directorio** de 1024 punteros (8 KB) a **tablas de
  nivel 2** de 1024 PTE. Una tabla de nivel 2 se crea con `calloc` en el primer fallo de
  su rango y se libera cuando `free` la deja vacía. Un espacio de 4 GB casi vacío
  cuesta ~8 KB en lugar de los 12 MB de una tabla plana.
- `PageTableEntry` (12 bytes):

| Campo | Bits | Uso |
|---|---|---|
| `frame` | 32 | Número de marco físico (válido si `valid`) |
| `valid` | 1 | La página está en RAM |
| `accessed` | 1 | Fue leída/escrita desde que se cargó |
| `dirty` | 1 | Fue escrita: se debe guardar al expulsarla |
| `swapped` / `swap_slot` | 1 / 32 | (extensión) existe una copia en swap y en qué ranura |

### 1.2 Otras estructuras

| Estructura | Implementación | Para qué |
|---|---|---|
| `PhysicalMemory` | Un bloque contiguo de `marcos × 4 KB` + pila de marcos libres + arreglo `owner[frame] = vpn` | El `owner` permite, al elegir un marco víctima, encontrar su PTE y invalidarla |
| `Swap` | Arreglo dinámico de buffers de página + lista de ranuras libres | Conserva el contenido de páginas sucias expulsadas |
| `RegionTable` | Arreglo ordenado de `{inicio, nº páginas}` + búsqueda binaria | `alloc` reserva sin usar RAM; distingue *fallo de página* (dirección reservada) de *segfault* (no reservada) |
| `FifoPolicy` | Lista doblemente enlazada intrusiva indexada por marco | Cola FIFO con todas las operaciones en O(1) |

## 2. Política de reemplazo FIFO

**Idea:** cuando no quedan marcos libres se expulsa la página que **lleva más tiempo residente**,
sin importar cuánto se haya usado.

**Implementación** (`policy/fifo.c`): `head` apunta al marco más antiguo y `tail` al más nuevo.

| Evento | Acción FIFO | Costo |
|---|---|---|
| `on_load(frame)` (página recién cargada) | Insertar al final | O(1) |
| `select_victim()` | Devolver `head` | O(1) |
| `on_release(frame)` (expulsión o `free`) | Desenlazar de cualquier posición | O(1) |
| `on_access(frame)` | **No hace nada** (FIFO ignora el uso) | O(1) |

**Flujo de un fallo con memoria llena** (`vm/page_fault.c`):

1. `pt_get_or_create` → crea la tabla de nivel 2 si hace falta.
2. `obtain_frame`: no hay marco libre → `policy->select_victim` → `evict_frame`.
3. `evict_frame`: si la víctima está **sucia** se escribe a swap (si está limpia se descarta);
   se invalida su PTE, se avisa a la política y se libera el marco. `replacements++`.
4. `fill_frame`: la página nueva se rellena desde swap (si fue expulsada antes) o con ceros.
5. Se actualiza la PTE (`valid=1`), el dueño del marco y la política (`on_load`).

## 3. Resultados de los programas de prueba

Las cargas están en `tests/workloads/` y son reproducibles (`tests/generate_workloads.sh`).

### 3.1 Programa 1 — Ejemplo del enunciado (`01_basico.txt`)

`alloc 8192  write 0 42  write 4096 99  read 0  read 4096`

| Accesos | Fallos | Hit rate | Reemplazos |
|---|---|---|---|
| 4 | 2 | 50,00 % | 0 |

Los 2 fallos son los primeros accesos a cada página (*paginación bajo demanda*); las lecturas
posteriores son aciertos y devuelven 42 y 99. Se crea 1 tabla de nivel 2.

### 3.2 Programa 2 — Recorrido cíclico de 96 páginas (`02_secuencial_ciclico.txt`)

Se reserva 384 KB, se escribe cada página una vez y luego se leen las 96 páginas en orden,
3 veces. Se ejecuta con 64 marcos y con 96 marcos.

| RAM | Accesos | Fallos | Hit rate | Reemplazos | Escrituras a swap | Lecturas de swap | EAT simulado |
|---|---|---|---|---|---|---|---|
| 256 KB (64 marcos) | 384 | **384** | **0,00 %** | 320 | 96 | 288 | 5 010 100 ns |
| 384 KB (96 marcos) | 384 | 96 | 75,00 % | 0 | 0 | 0 | 2 600 ns |

Valores esperados calculados a mano (y comprobados por `run_scenarios.sh`): con 64 marcos y un
ciclo de 96 páginas, cada acceso pide justo la página que FIFO acaba de expulsar → 384 fallos;
reemplazos = fallos − 64 marcos iniciales = 320. Con 96 marcos solo hay los 96 fallos
obligatorios (primer acceso).

Dos observaciones sobre swap: hay 96 escrituras (no 320) porque una página **limpia** se descarta
sin escribir y una sucia reutiliza su ranura; y las 288 lecturas de swap corresponden a las 288
lecturas del ciclo, todas verificadas contra el valor que se había escrito.

### 3.3 Programa 3 — Localidad de referencia (`03_localidad.txt`)

5 000 accesos (30 % escrituras) sobre 1 MB (256 páginas): 90 % van a un conjunto caliente de 16
páginas y 10 % a cualquiera. Barrido del tamaño de RAM:

| RAM (KB) | Marcos | Fallos | Hit rate | Reemplazos |
|---|---|---|---|---|
| 256 | 64 | 505 | 89,90 % | 441 |
| 320 | 80 | 445 | 91,10 % | 365 |
| 384 | 96 | 388 | 92,24 % | 292 |
| 512 | 128 | 325 | 93,50 % | 197 |
| 768 | 192 | 251 | 94,98 % | 59 |
| 1024 | 256 | 224 | 95,52 % | 0 |

Con 1 MB no hay reemplazos y los 224 fallos son exactamente las páginas distintas tocadas
(fallos obligatorios): es el techo del hit rate para esa carga.

### 3.4 Programa 4 — Anomalía de Belady (`04_belady.txt`)

Cadena de 24 referencias sobre 8 páginas, con páginas de 64 KB (`-s 65536`):

| Marcos | Fallos | Hit rate | Reemplazos |
|---|---|---|---|
| 4 (256 KB) | 14 | 41,67 % | 10 |
| 5 (320 KB) | **16** | **33,33 %** | 11 |

**Con más memoria FIFO fue peor.** Es la anomalía de Belady, propiedad conocida de FIFO.

### 3.5 Prueba de estrés y errores

- **Estrés** (`make stress`): 200 000 accesos aleatorios sobre 16 MB con 64 marcos → 196 853 fallos,
  hit rate 1,57 % (coincide con la teoría: 64/4096 = 1,56 % de probabilidad de acertar), 196 789
  reemplazos, 0,18 s de CPU, y **todas** las lecturas devolvieron el valor correcto.
- **Errores** (`05_errores.txt`): 6 comandos inválidos (segfault tras `free`, doble `free`, `free`
  de dirección intermedia, lectura fuera de reserva, `alloc 0`, `alloc` de 4 GB) se reportan por
  línea y la simulación continúa.
- **Memoria:** `valgrind --leak-check=full` sobre el simulador y sobre las pruebas unitarias:
  *All heap blocks were freed — 0 errors*.

## 4. Análisis: cambio del hit rate y de los reemplazos

1. **Más memoria ⇒ (normalmente) más hits y menos reemplazos.** En el programa 3, pasar de 64 a
   256 marcos sube el hit rate de 89,90 % a 95,52 % y baja los reemplazos de 441 a 0. La mejora
   tiene **rendimientos decrecientes**: los primeros 16 marcos extra ganan 1,2 puntos, los
   últimos 64 solo 0,5, porque ya cabe el conjunto caliente y solo se pelea por las páginas frías.
2. **Un hit rate "alto" puede ser muy caro.** En el programa 3 con 64 marcos el 10 % de fallos
   eleva el tiempo de acceso efectivo de 548 ns (sin reemplazos) a **404 110 ns** (×737), porque
   cada fallo con E/S de swap cuesta ~5 ms frente a 100 ns de un acceso a RAM. Esto justifica
   medir reemplazos y no solo hit rate.
3. **Peor caso: patrón cíclico mayor que la memoria** (programa 2). El hit rate es 0 % aunque
   solo falten 32 marcos: pasar de 64 a 96 marcos lo lleva a 75 % (el máximo posible, ya que 96 de
   384 accesos son fallos obligatorios). El comportamiento es un acantilado, no una curva suave.
4. **FIFO no aprovecha la localidad.** Como ignora los accesos, expulsa páginas calientes solo
   porque son antiguas: en el programa 3 con 64 marcos hubo 441 reemplazos aunque el 90 % de los
   accesos iba a solo 16 páginas.
5. **La anomalía de Belady** (programa 4) demuestra que en FIFO agregar marcos no garantiza menos
   fallos, lo que vuelve impredecible el ajuste de capacidad.
6. **Fallos obligatorios:** ninguna política puede evitar el primer acceso a cada página; en las
   cargas 02 y 03 fijan un piso de 96 y 224 fallos respectivamente.

## 5. Comparación teórica con otra política: LRU

| Aspecto | FIFO (implementada) | LRU |
|---|---|---|
| Criterio | Antigüedad **de carga** | Antigüedad **de último uso** |
| Aprovecha localidad temporal | No | Sí |
| Anomalía de Belady | **Sí** (programa 4) | No: es un *algoritmo de pila* (el conjunto residente con *n* marcos siempre está contenido en el de *n+1*) |
| Costo por acceso | Ninguno (ignora `on_access`) | Actualizar el orden en **cada** acceso (lista + tabla hash, o bits de referencia/timestamps en hardware) |
| Complejidad | Muy simple, determinista | Compleja; en la práctica se aproxima (reloj / segunda oportunidad) |
| Peor caso | Ciclo mayor que memoria | **El mismo** ciclo mayor que memoria |

Para cuantificarlo se implementó un modelo de referencia (Python, fuera del repositorio) que
recibe la misma secuencia de páginas de cada carga. Verificación: **su cálculo de FIFO coincide
exactamente con el del simulador** en las 10 configuraciones comparadas.

| Carga | Marcos | Fallos FIFO (simulador) | Fallos LRU (modelo) | Hit rate FIFO → LRU |
|---|---|---|---|---|
| 02 cíclico | 64 | 384 | 384 | 0,00 % → 0,00 % |
| 03 localidad | 64 | 505 | **390** | 89,90 % → 92,20 % |
| 03 localidad | 96 | 388 | 340 | 92,24 % → 93,20 % |
| 03 localidad | 128 | 325 | 295 | 93,50 % → 94,10 % |
| 03 localidad | 256 | 224 | 224 | 95,52 % → 95,52 % |
| 04 Belady | 4 | 14 | 18 | 41,67 % → 25,00 % |
| 04 Belady | 5 | 16 | 14 | 33,33 % → 41,67 % |

Conclusiones:

- **Con localidad (programa 3)** LRU reduce un 23 % los fallos con 64 marcos (505 → 390); la
  ventaja se estrecha al crecer la memoria y desaparece cuando todo cabe (256 marcos).
- **En el patrón cíclico (programa 2)** LRU no ayuda: empata con FIFO en 384 fallos (ambas
  expulsan justo la página que se necesitará a continuación).
- **LRU no es siempre mejor en una cadena concreta:** en la cadena de Belady con 4 marcos, LRU
  (18) pierde contra FIFO (14). Su superioridad es de **promedio sobre cargas con localidad**,
  no una garantía punto a punto. Lo que sí garantiza LRU es la **monotonía**: con 5 marcos baja
  a 14 fallos, mientras FIFO sube a 16.
- **Trade-off:** FIFO cuesta cero por acceso y es trivial de razonar; LRU compra menos fallos
  con localidad a cambio de trabajo en cada acceso, que es el costo dominante en hardware real
  (por eso los SO usan aproximaciones tipo reloj).

## 6. Cómo reproducir

En Linux se puede usar `make` directamente. En Windows, la compilación
equivalente es `mingw32-make`; produce `build/vmsim.exe`. Los objetivos que
ejecutan scripts (`test`, `stress` y `sweep.sh`) requieren MSYS2 o Git Bash,
además de las herramientas POSIX indicadas en el README. `valgrind` se ejecuta
en Linux.

```bash
make test                                     # 397 verificaciones unitarias + escenarios
tests/sweep.sh tests/workloads/03_localidad.txt          # tabla de §3.3
./build/vmsim -q -s 65536 -m 256 tests/workloads/04_belady.txt   # §3.4 (4 marcos)
./build/vmsim -q -s 65536 -m 320 tests/workloads/04_belady.txt   # §3.4 (5 marcos)
./build/vmsim -v tests/workloads/01_basico.txt           # traza de traducciones y fallos
make valgrind && make stress
```
