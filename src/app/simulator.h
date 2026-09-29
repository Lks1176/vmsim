/*
 * simulator.h - Orquestador de una simulación: abre la entrada, crea la memoria
 * virtual, ejecuta comando por comando y muestra las estadísticas.
 */
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "core/config.h"

#define EXIT_OK            0
#define EXIT_USAGE_ERROR   1
#define EXIT_INPUT_ERROR   2
#define EXIT_RUNTIME_ERROR 3

/* Ejecuta la simulación completa y devuelve el código de salida del proceso. */
int simulator_run(const Config *config);

#endif
