# Simulador de memoria virtual con paginación de 2 niveles (política FIFO)

## Integrantes del equipo
- Lukas Piedrahita Serna - CC 1040872196
- Felipe Uribe Holguin - CC 1026132431

##Descripción del proyecto

Laboratorio de Sistemas Operativos. Simula la traducción **VA → PA** con una tabla
de páginas de dos niveles, fallos de página con carga bajo demanda, un almacén de
respaldo (swap) y reemplazo de páginas **FIFO**.

- Lenguaje: **C99** (`gcc -Wall -Wextra -Werror -pedantic -std=c99`, sin warnings).
- Sin dependencias externas. Verificado con `valgrind`: **0 fugas, 0 errores**.
- Análisis y resultados: ver [`REPORTE.md`](REPORTE.md).

> **¿Por qué C y no C++?** El enunciado exige `-std=c99`, y para este problema
> structs opacos + punteros a función alcanzan para expresar todos los patrones
> (Strategy, Facade, Command, Factory). Se evita el ruido de plantillas,
> excepciones y RAII, lo que hace el código más fácil de explicar línea por línea.

---

## 1. Compilación y uso

### Linux

Requisitos: `gcc`, GNU Make y una shell POSIX. Desde la raíz del repositorio:

```bash
make                 # compila -> build/vmsim
make run             # ejecuta tests/workloads/01_basico.txt (el ejemplo del enunciado)
make run INPUT=tests/workloads/02_secuencial_ciclico.txt ARGS="-v"
make test            # pruebas unitarias + pruebas de escenarios
make valgrind        # verifica ausencia de fugas de memoria
make stress          # 200 000 accesos aleatorios con verificación de datos
make clean
```

### Windows con MinGW

Requisitos: MinGW-w64 (`gcc`) y `mingw32-make`. El ejecutable se genera como
`build/vmsim.exe`; las mismas variables `INPUT` y `ARGS` funcionan desde
`mingw32-make`:

```text
mingw32-make
mingw32-make run
mingw32-make run INPUT=tests/workloads/02_secuencial_ciclico.txt ARGS=-v
mingw32-make build/unit_tests.exe
build\unit_tests.exe
mingw32-make clean
```

Para ejecutar directamente desde `cmd.exe`:

```text
build\vmsim.exe tests\workloads\01_basico.txt
```

`mingw32-make test`, `stress` y los escenarios usan scripts POSIX. Para esos
objetivos en Windows se necesita MSYS2 o Git Bash con `sh`, `sed`, `awk` y
`head`; la compilación y la prueba unitaria no dependen de esa shell. El
objetivo `valgrind` está disponible en Linux; en Windows puede omitirse o
reemplazarse por una herramienta de análisis compatible con MinGW.

Ejecución directa:

```bash
./build/vmsim [opciones] [archivo]        # Linux; sin archivo lee de stdin
echo "alloc 8192 write 0 42 read 0" | ./build/vmsim
```

En Windows, use `build\vmsim.exe` en lugar de `./build/vmsim` y `type` o la
entrada interactiva de `cmd.exe` para proporcionar datos por stdin.

| Opción | Significado | Valor por defecto |
|---|---|---|
| `-m <KB>` | Memoria física en KB (mínimo 256, múltiplo del tamaño de página) | 256 |
| `-s <bytes>` | Tamaño de página, potencia de 2 entre 256 y 65536 | 4096 |
| `-p <politica>` | Política de reemplazo (hoy: `fifo`) | fifo |
| `-v` | Traza: cada traducción (PT1/PT2/offset), fallos y reemplazos | — |
| `-q` | Silencioso: solo estadísticas finales | — |
| `--mem-ns`, `--fault-ns`, `--disk-ns` | Modelo de costos del tiempo simulado (ns) | 100 / 10000 / 5000000 |

### Formato de entrada

```
alloc <bytes>            reserva espacio virtual e imprime la dirección asignada
write <dir> <valor>      escribe un byte (0-255) en la dirección virtual
read  <dir>              lee un byte e imprime su valor
free  <dir>              libera la reserva que EMPIEZA en <dir> (la dirección que devolvió alloc)
```

Los comandos pueden ir varios por línea o uno por línea; `#` inicia un comentario;
los números pueden ser decimales o hexadecimales (`0x1000`).

### Salida

```
===== Estadísticas finales =====
Total de accesos: N
Total fallos de página: M
Hit rate: (N-M)/N*100 %
Total reemplazos: K
Política: FIFO
--- detalle --- (lecturas/escrituras, swap, tablas de nivel 2, tiempo simulado, EAT, CPU real)
```

Códigos de salida: `0` correcto · `1` error de uso/configuración · `2` error de
entrada (archivo inexistente o sintaxis inválida) · `3` error interno/fatal.

---

## 2. Estructura del repositorio

```
vmsim/
├── Makefile
├── README.md / REPORTE.md
├── src/
│   ├── main.c                  Orquestador: config -> simulación -> código de salida
│   ├── core/                   Tipos y utilidades base, sin dependencias de dominio
│   │   ├── status.[ch]         Códigos de error (VmStatus)
│   │   ├── config.[ch]         Configuración, CLI y validación
│   │   ├── address.[ch]        Partición de bits de la VA (PT1 | PT2 | offset)
│   │   ├── stats.[ch]          Contadores y reporte final
│   │   └── numparse.[ch]       Conversión segura texto -> entero
│   ├── memory/                 Estructuras de datos (cada una es un tipo opaco)
│   │   ├── page_table.[ch]     Tabla de 2 niveles, PTE {frame, valid, accessed, dirty}
│   │   ├── physical_memory.[ch] RAM simulada en marcos + marcos libres
│   │   ├── swap.[ch]           Almacén de respaldo de páginas sucias
│   │   └── region_table.[ch]   Reservas de alloc (distingue fallo de página de segfault)
│   ├── policy/                 Política de reemplazo (intercambiable)
│   │   ├── replacement.[ch]    Interfaz (Strategy) + fábrica (Factory)
│   │   └── fifo.[ch]           Implementación FIFO
│   ├── vm/                     Lógica del sistema de memoria virtual
│   │   ├── virtual_memory.[ch] Fachada: alloc/free/read/write
│   │   ├── translation.[ch]    Traducción VA -> PA
│   │   └── page_fault.[ch]     Manejo de fallos y expulsión de víctimas
│   └── app/                    Capa de aplicación
│       ├── command_parser.[ch] Lector de comandos (Command)
│       └── simulator.[ch]      Bucle de ejecución y reporte
└── tests/
    ├── unit/test_vmsim.c       Pruebas unitarias por módulo (397 verificaciones)
    ├── run_scenarios.sh        Pruebas de escenarios con resultados calculados a mano
    ├── generate_workloads.sh   Genera las cargas 02, 03 y 04 (reproducibles)
    ├── stress.sh / sweep.sh    Estrés y barrido de tamaño de memoria
    └── workloads/              Programas de prueba (01 a 05)
```

**Reglas de dependencia entre capas** (solo se depende hacia abajo):
`main → app → vm → {memory, policy} → core`. `memory` y `policy` no se conocen
entre sí; `vm` es el único que los une.

---

## 3. Cómo funciona (resumen)

1. `alloc n` **reserva** páginas virtuales (asignador secuencial, alineado a página).
   No consume RAM: la asignación es **perezosa** (*demand paging*).
2. En cada `read/write`, `vm_translate` verifica que la VA esté reservada
   (si no: *segmentation fault*), recorre `dir[PT1] → PTE[PT2]` y, si la PTE no es
   válida, invoca el **manejador de fallos**.
3. El manejador crea la tabla de nivel 2 si no existe, obtiene un marco libre o,
   si no hay, pide a la política una **víctima** (la más antigua en FIFO), la
   expulsa (escribiéndola a swap solo si está *sucia*) y carga la página nueva
   (desde swap o en ceros).
4. La PA resultante es `frame * page_size + offset`; el dato se lee/escribe en
   la RAM simulada **por dirección física**, por lo que los valores realmente
   sobreviven a las expulsiones (probado en `tests/`).

### Decisiones de diseño y supuestos

| Tema | Decisión |
|---|---|
| Valor de `write` | Un byte (0-255). Evita que un valor de 4 bytes cruce el límite de página. |
| Qué es un "acceso" | Cada `read`/`write` **válido**. Los que terminan en segfault no cuentan. |
| Qué es un "fallo" | Todo acceso a una página no residente, incluido el **primer acceso** (paginación bajo demanda). |
| `free <dir>` | Solo acepta el inicio de una reserva (como `free(ptr)`). Libera marcos, swap y tablas de nivel 2 vacías. |
| Direcciones tras `free` | El asignador no reutiliza direcciones virtuales (simple y determinista); el espacio es de 4 GB. |
| Tamaño de página configurable | El offset usa `log2(page)` bits; los bits restantes se reparten PT2 = mitad, PT1 = resto. Con 4 KB da 10/10/12. |
| Errores en un comando | *Recuperables* (segfault, free inválido, alloc inválido): se informa `Línea N: ...` en stderr y se continúa. *Fatales* (sin memoria del host, invariante roto): se aborta con código 3 tras imprimir las estadísticas. |
| "Tiempo" | Se reporta el **tiempo simulado** (modelo: RAM 100 ns, fallo 10 µs, E/S de swap 5 ms, configurable), el tiempo de acceso efectivo (EAT) y el tiempo de CPU real del simulador. |

---

## 4. Buenas prácticas y patrones de diseño usados (y dónde)

### Patrones de diseño

| Patrón | Dónde | Qué resuelve |
|---|---|---|
| **Strategy** | `policy/replacement.h` (interfaz `ReplacementPolicy` con punteros a función) y `policy/fifo.c` | La política es intercambiable. `page_fault.c` llama a `policy->select_victim()` sin saber que es FIFO. Agregar LRU no modifica ningún otro archivo salvo la fábrica. |
| **Factory** | `replacement_create()` en `policy/replacement.c` | Un único lugar decide qué implementación construir según `PolicyType`. |
| **Facade** | `vm/virtual_memory.h` | El simulador solo ve `vm_alloc/free/read/write`; oculta tablas, marcos, swap y política. |
| **Command** | `app/command_parser.[ch]` + tabla de despacho `HANDLERS[]` en `app/simulator.c` | Cada línea se convierte en un objeto `Command` que se despacha por tipo, sin cadenas de `if/else`. Agregar un comando = una entrada en `COMMAND_SPECS` + un manejador. |
| **Tipo de dato abstracto (opaco)** | `PageTable`, `PhysicalMemory`, `Swap`, `RegionTable` (struct definido solo en el `.c`) | Encapsulamiento: nadie fuera del módulo toca su representación. |
| **Herencia por composición (C)** | `FifoPolicy` contiene `ReplacementPolicy base` como **primer campo** | Permite convertir `ReplacementPolicy*` ↔ `FifoPolicy*` de forma segura, como una clase base. |
| **Lista intrusiva** | `policy/fifo.c` (`next[]`/`prev[]` indexados por marco) | Cola FIFO con inserción, consulta y **borrado en cualquier posición en O(1)** (necesario cuando `free` libera marcos del medio). |

### Buenas prácticas

| Práctica | Dónde |
|---|---|
| **`main` sin lógica** | `main.c` solo llama a `config_from_args` y `simulator_run`. |
| **Separación de responsabilidades** | Traducción (`translation.c`), fallos (`page_fault.c`), reemplazo (`policy/`) están en archivos distintos, como pide el enunciado. |
| **Manejo de errores por valores de retorno** | Toda función falible devuelve `VmStatus`. Las librerías nunca llaman `exit()` ni imprimen: solo `simulator.c` decide cómo reportar. Se distingue error recuperable de fatal (`vm_status_is_fatal`). |
| **Validación en la frontera** | `config_validate` (RAM ≥ 256 KB, página potencia de 2, RAM múltiplo de página), `numparse.c` (rechaza signos, basura y desbordes; `atoi` no lo hace), parser (comandos desconocidos, argumentos faltantes, valor > 255). |
| **Programación defensiva** | `pm_free_frame` y `swap_free_slot` detectan doble liberación; `pm_read_byte` verifica límites de PA; `evict_frame` verifica que tabla y marco concuerden (`VM_ERR_INTERNAL`). |
| **Sin fugas, limpieza única** | Patrón `create/destroy` por módulo; todos los `*_destroy` toleran `NULL`, así `vm_create` limpia con un solo `goto fail` si algo falla a medias. Verificado con valgrind. |
| **Crecimiento seguro de arreglos** | `swap_grow` y `rt_reserve` usan `realloc` con puntero temporal (no pierden el bloque si falla). |
| **Sin números mágicos** | Constantes con nombre (`MIN_PHYS_MEM_BYTES`, `MAX_PAGE_SIZE`, `NIL`, ...); tablas de descriptores (`COMMAND_SPECS`, `OPTIONS`, `POLICY_TABLE`) en lugar de código repetido. |
| **`const` y `static`** | Funciones internas `static`; parámetros de solo lectura `const`. |
| **Include guards y dependencias automáticas** | Todos los `.h`; el Makefile usa `-MMD -MP` para recompilar si cambia un header. |
| **Pruebas en 3 niveles** | Unitarias (por módulo), escenarios (resultados calculados a mano, verificación de integridad de datos) y estrés (200 000 accesos). |
| **Determinismo** | FIFO es determinista y el marco libre se elige siempre en el mismo orden, por lo que las corridas son reproducibles. Las cargas aleatorias usan un generador con semilla fija. |
| **Optimización de E/S** | Una página **limpia** se descarta sin escribirla a swap; una sucia reutiliza su ranura de swap. |

---

## 5. Cómo extender: agregar LRU

1. Crear `src/policy/lru.c` con su `lru_create(num_frames)`. Estado sugerido: la misma
   lista intrusiva de FIFO, pero `on_access` **mueve el marco al final** de la lista.
   `select_victim`, `on_load` y `on_release` quedan igual.
2. Añadir `POLICY_LRU` al enum en `replacement.h`, una fila en `POLICY_TABLE` y un
   `case` en `replacement_create()` y `replacement_type_name()`.

No se modifica `translation.c`, `page_fault.c` ni el simulador: ya invocan `on_access`
en cada acceso precisamente para esto.

## 6. Limitaciones conocidas

- No hay TLB (fuera del alcance del enunciado; ver OSTEP cap. 19).
- El swap vive en memoria del host (no en disco real); su costo se modela con `--disk-ns`.
- El asignador virtual nunca reutiliza direcciones liberadas.
