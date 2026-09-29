#include "app/simulator.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "app/command_parser.h"
#include "vm/virtual_memory.h"

typedef struct Simulator {
    const Config *config;
    VirtualMemory *vm;
    unsigned long commands;
    unsigned long errors;
} Simulator;

/* Imprime en stdout si no se encuentra en modo silencioso. */
static void say(const Simulator *sim, const char *format, ...)
{
    if (sim->config->verbosity == VERBOSITY_QUIET) {
        return;
    }
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/* ---- Manejadores: uno por tipo de comando ------------------------------ */

static VmStatus do_alloc(Simulator *sim, const Command *cmd)
{
    uint32_t address;
    const VmStatus st = vm_alloc(sim->vm, cmd->bytes, &address);
    if (st == VM_OK) {
        say(sim, "alloc %-10u -> VA 0x%08X\n", cmd->bytes, address);
    }
    return st;
}

static VmStatus do_write(Simulator *sim, const Command *cmd)
{
    return vm_write(sim->vm, cmd->address, cmd->value);
}

static VmStatus do_read(Simulator *sim, const Command *cmd)
{
    uint8_t value;
    const VmStatus st = vm_read(sim->vm, cmd->address, &value);
    if (st == VM_OK) {
        say(sim, "read  VA 0x%08X -> %u\n", cmd->address, (unsigned)value);
    }
    return st;
}

static VmStatus do_free(Simulator *sim, const Command *cmd)
{
    const VmStatus st = vm_free(sim->vm, cmd->address);
    if (st == VM_OK) {
        say(sim, "free  VA 0x%08X -> liberada\n", cmd->address);
    }
    return st;
}

typedef VmStatus (*CommandHandler)(Simulator *, const Command *);

/* Tabla de despacho indexada por CommandType (evita un switch que crece). */
static const CommandHandler HANDLERS[CMD_TYPE_COUNT] = {
    [CMD_ALLOC] = do_alloc,
    [CMD_WRITE] = do_write,
    [CMD_READ]  = do_read,
    [CMD_FREE]  = do_free,
};

static VmStatus execute(Simulator *sim, const Command *cmd)
{
    if (cmd->type >= CMD_TYPE_COUNT || HANDLERS[cmd->type] == NULL) {
        return VM_ERR_INVALID_ARG;
    }
    sim->commands++;
    return HANDLERS[cmd->type](sim, cmd);
}

/* Bucle principal. Devuelve el código de salida. */
static int run_commands(Simulator *sim, FILE *input)
{
    CommandParser parser;
    Command cmd;
    parser_init(&parser, input);

    for (;;) {
        const ParseStatus ps = parser_next(&parser, &cmd);
        if (ps == PARSE_EOF) {
            return EXIT_OK;
        }
        if (ps == PARSE_ERROR) {
            fprintf(stderr, "Error de sintaxis en la línea %u: %s\n", parser.token_line,
                    parser_error(&parser));
            return EXIT_INPUT_ERROR;
        }

        const VmStatus st = execute(sim, &cmd);
        if (st == VM_OK) {
            continue;
        }
        sim->errors++;
        fprintf(stderr, "Línea %u: %s falló: %s\n", cmd.line, command_name(cmd.type),
                vm_status_str(st));
        if (vm_status_is_fatal(st)) {
            return EXIT_RUNTIME_ERROR; /* estado no confiable: abortar */
        }
        /* Error recuperable (segfault, free inválido...): se sigue con el siguiente. */
    }
}

int simulator_run(const Config *config)
{
    FILE *input = stdin;
    if (config->input_path != NULL) {
        input = fopen(config->input_path, "r");
        if (input == NULL) {
            fprintf(stderr, "No se pudo abrir '%s'\n", config->input_path);
            return EXIT_INPUT_ERROR;
        }
    }

    Simulator sim = {.config = config, .vm = NULL, .commands = 0, .errors = 0};
    const VmStatus st = vm_create(&sim.vm, config);
    if (st != VM_OK) {
        fprintf(stderr, "No se pudo crear la memoria virtual: %s\n", vm_status_str(st));
        if (input != stdin) {
            fclose(input);
        }
        return EXIT_RUNTIME_ERROR;
    }

    say(&sim, "Simulador de memoria virtual: página %u B, RAM %u KB (%u marcos), política %s\n",
        config->page_size, config->phys_mem_bytes / 1024u, config_num_frames(config),
        vm_policy_name(sim.vm));

    const clock_t started = clock();
    const int exit_code = run_commands(&sim, input);
    const double cpu_seconds = (double)(clock() - started) / (double)CLOCKS_PER_SEC;

    stats_print(vm_stats(sim.vm), vm_policy_name(sim.vm), cpu_seconds, stdout);
    if (sim.errors > 0) {
        printf("Comandos con error: %lu de %lu\n", sim.errors, sim.commands);
    }

    vm_destroy(sim.vm);
    if (input != stdin) {
        fclose(input);
    }
    return exit_code;
}
