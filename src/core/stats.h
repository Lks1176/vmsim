/*
 * stats.h - Contadores del simulador y su reporte final.
 */
#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Stats {
    uint64_t total_accesses;    /* lecturas + escrituras válidas            */
    uint64_t reads;
    uint64_t writes;
    uint64_t page_faults;
    uint64_t replacements;      /* fallos que obligaron a expulsar una página */
    uint64_t swap_outs;         /* páginas sucias escritas a swap            */
    uint64_t swap_ins;          /* páginas leídas desde swap                 */
    uint64_t l2_tables_created;
    uint64_t l2_tables_freed;
    uint64_t allocs;
    uint64_t frees;
    uint64_t simulated_ns;      /* tiempo simulado según el modelo de costos */
} Stats;

void stats_init(Stats *stats);
void stats_record_access(Stats *stats, bool is_write, uint64_t mem_ns);
void stats_record_fault(Stats *stats, uint64_t fault_ns);
void stats_record_swap_io(Stats *stats, bool is_write, uint64_t disk_ns);

/* (N - M) / N * 100; 0 si aún no hubo accesos. */
double stats_hit_rate(const Stats *stats);

void stats_print(const Stats *stats, const char *policy_name, double cpu_seconds, FILE *out);

#endif
