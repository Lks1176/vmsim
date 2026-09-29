/*
 * command_parser.h - Lee comandos desde un archivo (patrón COMMAND: cada línea de
 * texto se convierte en un objeto Command que otro módulo ejecuta).
 *
 * Formato: alloc <bytes> | write <dir> <valor> | read <dir> | free <dir>
 * Los comandos se separan por cualquier espacio en blanco, así que pueden ir
 * varios en una línea (como en el enunciado) o uno por línea. '#' inicia un
 * comentario hasta el fin de línea. Números en decimal o hexadecimal (0x...).
 */
#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stdio.h>

typedef enum CommandType {
    CMD_ALLOC = 0,
    CMD_WRITE,
    CMD_READ,
    CMD_FREE,
    CMD_TYPE_COUNT
} CommandType;

typedef struct Command {
    CommandType type;
    unsigned line;    /* línea del archivo donde empieza el comando */
    uint32_t bytes;   /* CMD_ALLOC */
    uint32_t address; /* CMD_WRITE, CMD_READ, CMD_FREE */
    uint8_t value;    /* CMD_WRITE */
} Command;

typedef enum ParseStatus { PARSE_OK, PARSE_EOF, PARSE_ERROR } ParseStatus;

#define PARSER_ERROR_MAX 160

typedef struct CommandParser {
    FILE *input;
    unsigned line;
    unsigned token_line;
    char error[PARSER_ERROR_MAX];
} CommandParser;

void parser_init(CommandParser *parser, FILE *input);

/* Lee el siguiente comando. */
ParseStatus parser_next(CommandParser *parser, Command *command);

const char *parser_error(const CommandParser *parser);
const char *command_name(CommandType type);

#endif
