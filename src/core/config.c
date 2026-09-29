#include "core/config.h"

#include <string.h>

#include "core/address.h"
#include "core/numparse.h"

void config_set_defaults(Config *config)
{
    config->page_size      = DEFAULT_PAGE_SIZE;
    config->phys_mem_bytes = DEFAULT_PHYS_MEM;
    config->policy         = POLICY_FIFO;
    config->verbosity      = VERBOSITY_NORMAL;
    config->mem_access_ns  = 100u;
    config->fault_ns       = 10000u;
    config->disk_io_ns     = 5000000u;
    config->input_path     = NULL;
}

const char *config_validate(const Config *config)
{
    if (config == NULL) {
        return "configuración nula";
    }
    const uint32_t ps = config->page_size;
    if (ps < MIN_PAGE_SIZE || ps > MAX_PAGE_SIZE || (ps & (ps - 1u)) != 0u) {
        return "el tamaño de página debe ser potencia de 2 entre 256 y 65536 bytes";
    }
    if (config->phys_mem_bytes < MIN_PHYS_MEM_BYTES) {
        return "la memoria física debe ser de al menos 256 KB";
    }
    if (config->phys_mem_bytes > MAX_PHYS_MEM_BYTES) {
        return "la memoria física no puede superar 1 GB";
    }
    if (config->phys_mem_bytes % ps != 0u) {
        return "la memoria física debe ser múltiplo del tamaño de página";
    }
    return NULL;
}

uint32_t config_num_frames(const Config *config)
{
    return config->phys_mem_bytes / config->page_size;
}


static bool set_mem_kb(Config *c, const char *v)
{
    uint64_t kb;
    if (!parse_u64(v, &kb) || kb > MAX_PHYS_MEM_BYTES / 1024u) {
        return false;
    }
    c->phys_mem_bytes = (uint32_t)(kb * 1024u);
    return true;
}

static bool set_page_size(Config *c, const char *v)
{
    return parse_u32(v, &c->page_size);
}

static bool set_policy(Config *c, const char *v)
{
    return replacement_parse_name(v, &c->policy);
}

static bool set_mem_ns(Config *c, const char *v)   { return parse_u64(v, &c->mem_access_ns); }
static bool set_fault_ns(Config *c, const char *v) { return parse_u64(v, &c->fault_ns); }
static bool set_disk_ns(Config *c, const char *v)  { return parse_u64(v, &c->disk_io_ns); }

typedef struct OptionSpec {
    const char *name;
    bool (*apply)(Config *, const char *);
} OptionSpec;

static const OptionSpec OPTIONS[] = {
    {"-m", set_mem_kb},
    {"-s", set_page_size},
    {"-p", set_policy},
    {"--mem-ns", set_mem_ns},
    {"--fault-ns", set_fault_ns},
    {"--disk-ns", set_disk_ns},
};

CliResult config_from_args(Config *config, int argc, char **argv)
{
    const char *prog = argc > 0 ? argv[0] : "vmsim";
    config_set_defaults(config);

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            return CLI_HELP;
        }
        if (strcmp(arg, "-v") == 0) {
            config->verbosity = VERBOSITY_VERBOSE;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            config->verbosity = VERBOSITY_QUIET;
            continue;
        }

        const OptionSpec *spec = NULL;
        for (size_t k = 0; k < sizeof OPTIONS / sizeof OPTIONS[0]; k++) {
            if (strcmp(arg, OPTIONS[k].name) == 0) {
                spec = &OPTIONS[k];
                break;
            }
        }
        if (spec != NULL) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: la opción %s requiere un valor\n", prog, arg);
                return CLI_ERROR;
            }
            const char *value = argv[++i];
            if (!spec->apply(config, value)) {
                fprintf(stderr, "%s: valor inválido '%s' para %s\n", prog, value, arg);
                return CLI_ERROR;
            }
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "%s: opción desconocida '%s'\n", prog, arg);
            return CLI_ERROR;
        }
        if (config->input_path != NULL) {
            fprintf(stderr, "%s: solo se admite un archivo de entrada\n", prog);
            return CLI_ERROR;
        }
        config->input_path = arg;
    }

    const char *problem = config_validate(config);
    if (problem != NULL) {
        fprintf(stderr, "%s: configuración inválida: %s\n", prog, problem);
        return CLI_ERROR;
    }
    return CLI_RUN;
}

void config_print_usage(const char *program, FILE *out)
{
    fprintf(out,
            "Uso: %s [opciones] [archivo_de_comandos]\n"
            "  Sin archivo, lee los comandos de la entrada estándar.\n\n"
            "Comandos: alloc <bytes> | write <dir> <valor 0-255> | read <dir> | free <dir>\n\n"
            "Opciones:\n"
            "  -m <KB>          memoria física en KB (mín. 256, def. 256)\n"
            "  -s <bytes>       tamaño de página, potencia de 2 en [256, 65536] (def. 4096)\n"
            "  -p <politica>    política de reemplazo: fifo (def. fifo)\n"
            "  -v               traza detallada de traducciones, fallos y reemplazos\n"
            "  -q               silencioso: solo estadísticas finales\n"
            "  --mem-ns <n>     costo simulado de un acceso a RAM (def. 100)\n"
            "  --fault-ns <n>   sobrecosto simulado de un fallo de página (def. 10000)\n"
            "  --disk-ns <n>    costo simulado de una E/S a swap (def. 5000000)\n"
            "  -h, --help       muestra esta ayuda\n",
            program);
}
