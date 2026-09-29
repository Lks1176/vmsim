/*
 * main.c -> No contiene lógica de negocio: solo orquesta
 *   Primero lee la configuración, luego ejecuta la simulación y finalmente devuelve el código.
 */
#include <stdio.h>

#include "app/simulator.h"
#include "core/config.h"

int main(int argc, char **argv)
{
    Config config;

    switch (config_from_args(&config, argc, argv)) {
    case CLI_RUN:
        return simulator_run(&config);
    case CLI_HELP:
        config_print_usage(argv[0], stdout);
        return EXIT_OK;
    case CLI_ERROR:
    default:
        config_print_usage(argv[0], stderr);
        return EXIT_USAGE_ERROR;
    }
}
