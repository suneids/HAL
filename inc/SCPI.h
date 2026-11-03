#ifndef SCPI_H
#define SCPI_H

#include "usart.h"



typedef enum{
    SCPI_COMMAND,
    SCPI_QUERY
}SCPI_Operation;

typedef enum{
    SCPI_ARG_NONE,
    SCPI_ARG_REQUIRED,
    SCPI_ARG_OPTIONAL
}SCPI_Argument;

/** @brief Command names must be uppercase. */
typedef struct{
    const char *name;
    uint16_t id;
    SCPI_Operation operation;
    SCPI_Argument argument;
}SCPI_CommandDef;

typedef struct{
    uint16_t id;
    uint8_t has_value;
    char value[32];
}SCPI_ParsedCommand;

typedef struct{
    USART_TypeDef *USARTx;
    uint32_t baud;
    uint8_t parity;
    uint8_t stop;
    const SCPI_CommandDef *available_commands;
    const uint8_t command_count;
}SCPI_cfg;

typedef enum{
    SCPI_ERR_NONE,
    SCPI_ERR_SYNTAX,
    SCPI_ERR_UNKNOWN,
    SCPI_ERR_ARGUMENT,
    SCPI_ERR_OVERFLOW,
    SCPI_ERR_QUEUE_FULL
}SCPI_Error;



void SCPI_Init(SCPI_cfg *cfg);
void SCPI_FeedByte(uint8_t byte);
void SCPI_Write(const char *data, uint16_t len);

void SCPI_Process();
uint8_t SCPI_PopCommand(SCPI_ParsedCommand *cmd);
SCPI_Error SCPI_CheckError(void);
#endif
