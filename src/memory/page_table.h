/*
 * page_table.h - Tabla de páginas de dos niveles (tipo de dato opaco).
 *
 *   directorio (nivel 1)          tablas de nivel 2 (se crean bajo demanda)
 *   +-----------+                 +---------------------------+
 *   | dir[PT1]  |--------------->| PTE[PT2] {frame, V, A, D} |
 *   +-----------+                 +---------------------------+
 *
 * Una entrada de nivel 1 es NULL hasta que se toca por primera vez una página
 * de ese rango, así un espacio de 4 GB casi vacío ocupa muy poca memoria.
 */
#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/address.h"

typedef struct PageTableEntry {
    uint32_t frame;     /* # de página física (solo válido si valid == 1)   */
    uint32_t swap_slot; /* ranura en swap (solo válido si swapped == 1)      */
    unsigned valid    : 1; /* la página está en memoria física              */
    unsigned accessed : 1; /* fue leída o escrita desde que se cargó        */
    unsigned dirty    : 1; /* fue escrita: hay que guardarla al expulsarla  */
    unsigned swapped  : 1; /* existe una copia en swap                      */
} PageTableEntry;

typedef struct PageTable PageTable;

PageTable *pt_create(const AddressLayout *layout);
void pt_destroy(PageTable *pt); /* seguro con NULL */

/* Devuelve la PTE de `va` o NULL si su tabla de nivel 2 aún no existe. */
PageTableEntry *pt_lookup(const PageTable *pt, uint32_t va);

/*
 * Devuelve la PTE de `va` creando la tabla de nivel 2 si hace falta.
 * NULL solo si no hay memoria. `created` (opcional) indica si se creó una tabla.
 */
PageTableEntry *pt_get_or_create(PageTable *pt, uint32_t va, bool *created);

/*
 * Pone en cero las PTE de `num_pages` páginas desde `start_va` y libera las
 * tablas de nivel 2 que queden vacías. Devuelve cuántas tablas liberó.
 */
size_t pt_release_range(PageTable *pt, uint32_t start_va, uint32_t num_pages);

#endif
