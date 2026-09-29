#include "core/stats.h"

#include <inttypes.h>
#include <string.h>

void stats_init(Stats *stats)
{
    memset(stats, 0, sizeof *stats);
}

void stats_record_access(Stats *stats, bool is_write, uint64_t mem_ns)
{
    stats->total_accesses++;
    if (is_write) {
        stats->writes++;
    } else {
        stats->reads++;
    }
    stats->simulated_ns += mem_ns;
}

void stats_record_fault(Stats *stats, uint64_t fault_ns)
{
    stats->page_faults++;
    stats->simulated_ns += fault_ns;
}

void stats_record_swap_io(Stats *stats, bool is_write, uint64_t disk_ns)
{
    if (is_write) {
        stats->swap_outs++;
    } else {
        stats->swap_ins++;
    }
    stats->simulated_ns += disk_ns;
}

double stats_hit_rate(const Stats *stats)
{
    if (stats->total_accesses == 0) {
        return 0.0;
    }
    return (double)(stats->total_accesses - stats->page_faults) * 100.0 /
           (double)stats->total_accesses;
}

void stats_print(const Stats *stats, const char *policy_name, double cpu_seconds, FILE *out)
{
    const double eat = stats->total_accesses
                           ? (double)stats->simulated_ns / (double)stats->total_accesses
                           : 0.0;

    fprintf(out, "\n===== Estadísticas finales =====\n");
    fprintf(out, "Total de accesos: %" PRIu64 "\n", stats->total_accesses);
    fprintf(out, "Total fallos de página: %" PRIu64 "\n", stats->page_faults);
    fprintf(out, "Hit rate: %.2f%%\n", stats_hit_rate(stats));
    fprintf(out, "Total reemplazos: %" PRIu64 "\n", stats->replacements);
    fprintf(out, "Política: %s\n", policy_name);
    fprintf(out, "--- detalle ---\n");
    fprintf(out, "Lecturas / escrituras: %" PRIu64 " / %" PRIu64 "\n", stats->reads, stats->writes);
    fprintf(out, "Páginas escritas a swap (sucias): %" PRIu64 "\n", stats->swap_outs);
    fprintf(out, "Páginas leídas desde swap: %" PRIu64 "\n", stats->swap_ins);
    fprintf(out, "Tablas de nivel 2 creadas / liberadas: %" PRIu64 " / %" PRIu64 "\n",
            stats->l2_tables_created, stats->l2_tables_freed);
    fprintf(out, "Reservas (alloc) / liberaciones (free): %" PRIu64 " / %" PRIu64 "\n",
            stats->allocs, stats->frees);
    fprintf(out, "Tiempo simulado total: %.3f ms\n", (double)stats->simulated_ns / 1e6);
    fprintf(out, "Tiempo de acceso efectivo (EAT): %.1f ns\n", eat);
    fprintf(out, "Tiempo de CPU real del simulador: %.3f s\n", cpu_seconds);
}
