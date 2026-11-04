#include "../inc/SCPI.h"
#include <string.h>
#include <ctype.h>
#define SCPI_BUFFER_SIZE 128
#define SCPI_QUEUE_SIZE 8
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

static volatile SCPI_Error scpi_error = SCPI_ERR_NONE;
static SCPI_ParsedCommand queue[SCPI_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;
static uint8_t queue_count = 0;

static char rx_buffer[SCPI_BUFFER_SIZE];
static volatile uint16_t rx_length = 0;
static volatile uint8_t rx_ready = 0;
static uint8_t rx_overflow = 0;

static SCPI_cfg *config;
static const uint32_t UART_SPEEDS[] = {9600, 19200, 38400, 57600, 115200};

static void SCPI_RxCallback(UART_HandleTypeDef *huart, uint8_t byte){
    SCPI_FeedByte(byte);
}


static void SCPI_SetError(SCPI_Error error){
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(scpi_error == SCPI_ERR_NONE){
        scpi_error = error;
    }

    __set_PRIMASK(primask);
}


static void ReconfigureUARTCFG(void)
{
	uint8_t parity = (*config->uart_cfg_reg >> 4) & 0b11;
	if (parity == 3) parity = 0;

	uint8_t stop2_flag = (*config->uart_cfg_reg >> 6) & 0b1;

	uint8_t parity_bit   = (parity != 0) ? 1 : 0;
	uint8_t stop_bits    = stop2_flag ? 2 : 1;

	uint16_t idx = *config->uart_cfg_reg & 0b1111;
	if (idx >= ARRAY_LEN(UART_SPEEDS)) idx = 0;
	uint32_t baud = UART_SPEEDS[idx];


	USART_Init(config->USARTx, baud, parity_bit, stop_bits);
}


void SCPI_Init(SCPI_cfg *cfg){
    config = cfg;
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;

    rx_length = 0;
    rx_ready = 0;
    rx_overflow = 0;
    scpi_error = SCPI_ERR_NONE;

    ReconfigureUARTCFG();
    USART_SetHandlers(config->USARTx, SCPI_RxCallback, NULL, NULL);
}


void SCPI_FeedByte(uint8_t byte){
    if(rx_ready) return;

    if(byte == '\r') return;

    if(byte == '\n'){
        if(rx_overflow){
            rx_overflow = 0;
            rx_length = 0;
            return;
        }

        if(rx_length > 0){
            rx_buffer[rx_length] = '\0';
            rx_ready = 1;
        }
        return;
    }

    if(rx_overflow) return;

    if(rx_length < SCPI_BUFFER_SIZE - 1){
        rx_buffer[rx_length++] = (char)byte;
    }
    else{
        rx_overflow = 1;
        SCPI_SetError(SCPI_ERR_OVERFLOW);
        rx_length = 0;
    }
}


void SCPI_Process(void){
    if(!rx_ready) return;

    char frame[SCPI_BUFFER_SIZE];

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t len = rx_length;
    memcpy(frame, rx_buffer, len);
    frame[len] = '\0';

    rx_length = 0;
    rx_ready = 0;

    __set_PRIMASK(primask);

    char title[SCPI_BUFFER_SIZE] = "";
    char full[SCPI_BUFFER_SIZE];
    char *cursor = frame;

    while(cursor && *cursor){
        char *part = cursor;
        char *separator = strchr(part, ';');

        if(separator){
            *separator = '\0';
            cursor = separator + 1;
        }
        else cursor = NULL;

        // Убираем пробелы по краям
        while(isspace((unsigned char)*part)) part++;

        size_t len = strlen(part);
        while(len && isspace((unsigned char)part[len - 1])){
            part[--len] = '\0';
        }

        if(!len){
        	SCPI_SetError(SCPI_ERR_SYNTAX);
            return;
        }

        // Отделяем аргумент от заголовка команды
        char *value = part;

        while(*value && !isspace((unsigned char)*value)) value++;

        uint8_t has_value = (*value != '\0');

        if(has_value){
            *value++ = '\0';
            while(isspace((unsigned char)*value)) value++;
            has_value = (*value != '\0');
        }

        // Определяем запрос
        len = strlen(part);
        uint8_t is_query = (part[len - 1] == '?');

        if(is_query) part[--len] = '\0';

        if(!len){
            SCPI_SetError(SCPI_ERR_SYNTAX);
            return;
        }

        // Нормализуем имя команды
        for(char *p = part; *p; p++){
            *p = (char)toupper((unsigned char)*p);
        }

        // Собираем полный путь
        full[0] = '\0';

        uint8_t common = (part[0] == '*');
        uint8_t absolute = (part[0] == ':');

        char *command = absolute ? part + 1 : part;

        size_t prefix_len = (!common && !absolute) ? strlen(title) : 0;
        size_t command_len = strlen(command);

        if(prefix_len + (prefix_len != 0) + command_len >= sizeof(full)){
            SCPI_SetError(SCPI_ERR_OVERFLOW);
            return;
        }

        if(prefix_len){
            memcpy(full, title, prefix_len);
            full[prefix_len++] = ':';
        }

        memcpy(full + prefix_len, command, command_len + 1);

        // Обновляем контекст подсистемы
        if(!common){
            char *last = strrchr(full, ':');

            if(last){
                size_t size = (size_t)(last - full);
                memcpy(title, full, size);
                title[size] = '\0';
            }
            else title[0] = '\0';
        }

        // Ищем готовую команду в таблице приложения
        const SCPI_CommandDef *def = NULL;

        for(uint16_t i = 0; i < config->command_count; i++){
            const SCPI_CommandDef *item = &config->available_commands[i];

            if(strcmp(item->name, full) != 0) continue;

            if(item->operation != (is_query ? SCPI_QUERY : SCPI_COMMAND))
                continue;

            def = item;
            break;
        }

        if(!def){
        	SCPI_SetError(SCPI_ERR_UNKNOWN);
            return;
        }

        // Проверяем наличие аргумента
        if((def->argument == SCPI_ARG_REQUIRED && !has_value) || (def->argument == SCPI_ARG_NONE && has_value)){
            SCPI_SetError(SCPI_ERR_ARGUMENT);
            return;
        }

        if(has_value && strlen(value) >= sizeof(queue[0].value)){
        	SCPI_SetError(SCPI_ERR_OVERFLOW);
            return;
        }

        if(queue_count >= SCPI_QUEUE_SIZE){
        	SCPI_SetError(SCPI_ERR_QUEUE_FULL);
            return;
        }

        // Push в FIFO
        SCPI_ParsedCommand *out = &queue[queue_head];

        out->id = def->id;
        out->has_value = has_value;

        if(has_value) strcpy(out->value, value);
        else out->value[0] = '\0';

        queue_head = (queue_head + 1) % SCPI_QUEUE_SIZE;
        queue_count++;
    }
}



uint8_t SCPI_PopCommand(SCPI_ParsedCommand *cmd){
    if(!cmd || !queue_count) return 0;

    *cmd = queue[queue_tail];

    queue_tail = (queue_tail + 1) % SCPI_QUEUE_SIZE;
    queue_count--;

    return 1;
}




SCPI_Error SCPI_CheckError(void){
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    SCPI_Error error = scpi_error;
    scpi_error = SCPI_ERR_NONE;

    __set_PRIMASK(primask);
    return error;
}

void SCPI_Write(const char *data, uint16_t len){
    USART_WriteLine(config->USARTx, data, len);
}

