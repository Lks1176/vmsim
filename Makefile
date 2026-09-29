# Makefile - Simulador de memoria virtual (paginación de 2 niveles, FIFO)
#
# Targets:
#   make            compila el simulador en build/vmsim
#   make run        ejecuta el simulador (INPUT=archivo ARGS="-v -m 512")
#   make test       pruebas unitarias + pruebas de escenarios
#   make valgrind   ejecuta cargas de trabajo bajo valgrind (0 fugas esperadas)
#   make stress     prueba de estrés (200 000 accesos aleatorios)
#   make clean      borra build/

CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -pedantic -std=c99 -O2 -g -Isrc
BUILD    = build

ifeq ($(OS),Windows_NT)
EXE      = .exe
ifneq (,$(MSYSTEM))
MKDIR_P  = mkdir -p "$(@D)"
RM_RF    = rm -rf "$(BUILD)"
else
MKDIR_P  = if not exist "$(@D)" mkdir "$(@D)"
RM_RF    = rmdir /S /Q "$(BUILD)" 2>NUL || exit 0
endif
RUN      =
else
EXE      =
MKDIR_P  = mkdir -p "$(@D)"
RM_RF    = rm -rf "$(BUILD)"
RUN      = ./
endif

TARGET   = $(BUILD)/vmsim$(EXE)
UNIT     = $(BUILD)/unit_tests$(EXE)

SRCS     = $(wildcard src/*.c src/*/*.c)
OBJS     = $(patsubst src/%.c,$(BUILD)/obj/%.o,$(SRCS))
LIB_OBJS = $(filter-out $(BUILD)/obj/main.o,$(OBJS))

INPUT   ?= tests/workloads/01_basico.txt
ARGS    ?=

.PHONY: all clean run test valgrind stress

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) $^ -o $@

# Compilación con dependencias automáticas (-MMD): recompila si cambia un .h
$(BUILD)/obj/%.o: src/%.c
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(UNIT): tests/unit/test_vmsim.c $(LIB_OBJS)
	@$(MKDIR_P)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	$(RUN)$(TARGET) $(ARGS) $(INPUT)

test: $(UNIT) $(TARGET)
	$(RUN)$(UNIT)
	sh tests/run_scenarios.sh

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 \
	    $(RUN)$(TARGET) -q tests/workloads/02_secuencial_ciclico.txt
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 \
	    $(RUN)$(TARGET) -q tests/workloads/05_errores.txt

stress: $(TARGET)
	sh tests/stress.sh

clean:
	$(RM_RF)

-include $(OBJS:.o=.d)
