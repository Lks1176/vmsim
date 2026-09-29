/*
 * config.h - Configuración del simulador y lectura de la línea de comandos.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdio.h>

#include "policy/replacement.h"

#define DEFAULT_PAGE_SIZE     4096u
#define DEFAULT_PHYS_MEM      (256u * 1024u)
#define MIN_PHYS_MEM_BYTES    (256u * 1024u)
#define MAX_PHYS_MEM_BYTES    (1024u * 1024u * 1024u)

typedef enum Verbosity {
    VERBOSITY_QUIET = 0, /* solo estadísticas finales               */
    VERBOSITY_NORMAL,    /* + resultado de alloc/read/free          */
    VERBOSITY_VERBOSE    /* + traza de traducciones y fallos        */
} Verbosity;

typedef struct Config {
    uint32_t page_size;
    uint32_t phys_mem_bytes;
    PolicyType policy;
    Verbosity verbosity;
    /* Modelo de costos para el "tiempo" simulado (nanosegundos). */
    uint64_t mem_access_ns; /* un acceso a RAM                     */
    uint64_t fault_ns;      /* sobrecosto de atender un fallo      */
    uint64_t disk_io_ns;    /* una lectura o escritura a swap      */
    const char *input_path; /* NULL = leer de stdin                */
} Config;

typedef enum CliResult { CLI_RUN, CLI_HELP, CLI_ERROR } CliResult;

void config_set_defaults(Config *config);

/* Parsea argv. Imprime los errores de uso en stderr. */
CliResult config_from_args(Config *config, int argc, char **argv);

/* NULL si es válida; si no, mensaje que explica el problema. */
const char *config_validate(const Config *config);

uint32_t config_num_frames(const Config *config);
void config_print_usage(const char *program, FILE *out);

#endif
