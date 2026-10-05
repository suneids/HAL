Selfwritten High Abstractive Layer for STM32F103C6T6
Instruction for using modules:

---

<h2 align=center>GPIO</h2>

| Functions | Description |
| --------- | ----- |
| void pinMode(Pin_t pin, uint8_t mode, uint8_t cnf, uint8_t pull) | Configures one pin. Adjusts the MODE and CNF bits in the CRL/CRH registers, including Pull-up/down control. |
| void pinModeMulti(Pin_t* pins, size_t pins_number, uint8_t mode, uint8_t cnf, uint8_t pull) | Group initialization of an array of pins with the same parameters. Optimizes the code when configuring data buses |
| void digitalWrite(Pin_t pin, uint8_t value) | Sets the output status (High/Low). Uses the BSRR register to atomically change the state of a bit. |
| uint8_t digitalRead(Pin_t pin) | Returns the current logical state of the pin by reading the value from the input data register (IDR). |
| void pinToggle(Pin_t pin) | Inverts the current state of the output pin. It is convenient for debugging and display (Heartbeat LED). |

## Code example: GPIO
```C
  // LED on PC13
  enableGPIOClock(GPIOC);
  pinMode(PC13, GPIO_MODE_OUTPUT_50MHZ, GPIO_CNF_OUT_PUSH_PULL, GPIO_PULL_NONE);
```

---

<h2 align=center>USART</h2>

| Functions | Description |
| --------- | ----- |
| void echo(USART_TypeDef *USARTx) | A debug utility that immediately retransmits received data back to the sender. Useful for link testing. |
| void usartInit(USART_TypeDef *USARTx, uint32_t baud_rate) | High-level initializer. Configures baud rate, parity, and stop bits, then enables the USART peripheral. | 
| uint16_t usartAvailable(USART_TypeDef *USARTx) | Returns the number of bytes currently waiting in the receive buffer. Essential for non-blocking reads. |
| void usartWriteByte(USART_TypeDef *USARTx, char byte) | Transmits a single 8-bit character. Waits for the TXE flag to ensure the register is ready. |
| void usartWriteLine(USART_TypeDef *USARTx, const char *str) | Transmits a null-terminated string followed by line ending characters (CR/LF). |
| char usartReadByte(USART_TypeDef *USARTx) | Reads a single byte from the DR register. Blocks until the RXNE flag is set. |
| void usartReadBytes(USART_TypeDef *USARTx, char *buf, uint32_t max_len) | Reads a stream of data into a buffer until max_len is reached or a timeout occurs. |
| void USART_IRQHandler_Generic(USART_TypeDef *USARTx) | A centralized interrupt handler that manages RX/TX events for any given USART instance |
| uint32_t usartDiv(uint32_t F_CPU, uint32_t baud_rate) | Internal helper. Calculates the required USARTDIV value based on $f_{CPU}$ and target baud rate. |
| uint8_t usartIndex(USART_TypeDef *USARTx) | Returns the hardware instance index (0 for USART1, etc.). Used for peripheral mapping and offsets. |

## How to use
> [!NOTE]
>- **Clocking**: The usartInit function internally handles the calculation of the USARTDIV using the usartDiv helper, based on the current $f_{CPU}$.
>- **Interrupts**: If you plan to use USART_IRQHandler_Generic, ensure you have enabled the USART interrupt in the NVIC and set the RXNEIE bit in the CR1 register.
>- **Debug**: Use echo(USART1) in your while(1) loop to verify that your hardware connection (TX/RX lines) and baud rate settings are correct.

## Code Example: USART
```C
// Initialize USART1 at 115200 baud
usartInit(USART1, 115200);

// Simple log message
usartWriteLine(USART1, "System Started. Hexapod MK3 online.");

// Non-blocking read example
if (usartAvailable(USART1)) {
    char data = usartReadByte(USART1);
    // Process incoming command
}
```

---

<h2 align=center>TIM</h2>

| Functions | Description |
| --------- | ----- |
| void sysTickInit(void) | Configures the system tick timer to generate interrupts every 1ms. Essential for time-base management. |
| uint32_t millis(void) | Returns the number of milliseconds elapsed since the program started. Used for non-blocking delays and timing. |
| void timerInit(TIM_TypeDef *TIMx, uint32_t psc, uint32_t arr, uint8_t debounce) | General-purpose timer initializer. Sets Prescaler (PSC) and Auto-Reload (ARR) values, with optional debounce filtering. |
| void sysTickDelay(uint32_t ms) | Implements a blocking delay in milliseconds using the SysTick counter. Accurate but halts main execution. |
| void timRegisterHandler(TIM_TypeDef *TIMx, TimHandler_t handler) | Attaches a callback function (handler) to a specific timer instance. Enables modular interrupt processing. |

## How to use
> [!NOTE]
>- **Timebase**: sysTickInit() must be called at the very beginning of main(). It populates the global tick counter used by millis() and sysTickDelay().
>- **Timer Calculation**: For a 72MHz clock, PSC=7199 gives a 10kHz timer frequency. The ARR value then determines the overflow period.
>- **Interrupts**: Use timRegisterHandler() to link your logic (e.g., motor control or sensor sampling) to a hardware timer without modifying the core HAL files.

## Code Example: TIM & SysTick
```C
// Initialize System Tick for millis() support
sysTickInit();

// Set up TIM2 to trigger every 100ms
// Formula: Update_Event = f_CK_PSC / ((PSC + 1) * (ARR + 1))
timerInit(TIM2, 7199, 999, 0); 

while(1) {
    static uint32_t last_time = 0;
    if (millis() - last_time >= 500) {
        last_time = millis();
        pinToggle(PC13); // Heartbeat every 500ms
    }
}
```

---

<h2 align=center>PWM</h2>

| Functions | Description |
| --------- | ----- |
| void pwmInit(Pin_t pin) | Configures the GPIO pin for Alternate Function and initializes the corresponding Timer channel in PWM mode. |
| void pwmWrite(Pin_t pin, uint16_t value) | Updates the Capture/Compare Register (CCR) to set the pulse width, effectively controlling motor speed or LED brightness. |
| TimerChannel_t getTIMChannel(Pin_t pin) | Internal helper. Maps a specific GPIO pin to its hardware Timer channel based on the STM32 pinout. |

## How to use
> [!NOTE]
>- **Auto-Mapping**: The pwmInit uses getTIMChannel to automatically configure the correct Timer and Channel associated with the pin. No need to look up datasheets for every pin.
>- **Resolution**: The range of the value in pwmWrite depends on the ARR register setting of the underlying Timer.
>- **Hardware**: Ensure the pin supports Alternate Function (AF) for the specific timer channel.

## Code Example: PWM
```C
// Initialize PWM on a specific pin (e.g., PA0 - TIM2 Channel 1)
pwmInit(PA0);

// Set duty cycle (0 to ARR value)
pwmWrite(PA0, 500); 

// Example: Breathing LED effect
while(1) {
    uint32_t duty = (millis() / 10) % 1000;
    pwmWrite(PA0, duty);
}
```

---

<h2 align=center>I2C</h2>

| Functions | Description |
| --------- | ----- |
| void I2C_Init(I2C_TypeDef *I2Cx, Pin_t SDA, Pin_t SCL) | Sets up the hardware I2C peripheral, including clock speed and GPIO pins configuration (SDA/SCL). |
| void I2C_Start(I2C_TypeDef *I2Cx) | Generates a START condition on the bus. Manages the SB (Start Bit) flag in the Status Register. |
| void I2C_WriteReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t data) | Writes a single byte to a specific register of a slave device. Handles addressing and data transmission. |
| void I2C_WriteByteArray(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pData, uint16_t len) | Transmits multiple bytes to a slave device. Efficient for loading configuration tables or large data sets. |
| uint8_t I2C_ReadReg(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr) | Reads a single byte from a target register. Manages the transition between Write (addressing) and Read modes. |
| void I2C_Read_Burst(I2C_TypeDef *I2Cx, uint8_t devAddr, uint8_t regAddr, uint8_t *pBuffer, uint16_t size) | High-performance sequential read. Reads multiple bytes in a single transaction, essential for IMU sensor data. |

## Hardware constraint (PB5 / I2C1_SMBA)
> [!CAUTION]
> PB5 is mapped to I2C1_SMBA (SMBus Alert).<br/>
> When I2C1 is enabled (I2C_CR1.PE = 1), the peripheral takes control of this pin via Alternate Function.<br/>
> As a result, TIM3_CH2 (PWM) or GPIO usage on PB5 will stop working.<br/>
> Do not use PB5 for PWM if I2C1 is active.

## How to use
> [!NOTE]
>- **Addressing**: Note that the library expects a 7-bit address. You must shift it left by 1 bit (address << 1) before passing it to the functions, or ensure the library handles it internally.
>- **Blocking Operations**: These functions wait for hardware flags (like SB, ADDR, RXNE). If the bus is physically broken, it may lead to a hang. Use with caution in critical systems.
>- **Burst Read**: I2C_Read_Burst is optimized for reading IMU data. It automatically manages the ACKing of bytes and generates a NACK followed by a STOP condition after the last byte.

## Code Example: I2C (MPU9250 Example)
```C
// Initialize I2C1 with PA9 (SDA) and PA10 (SCL)
I2C_Init(I2C1, PA9, PA10);

// Writing to a register (e.g., wake up sensor)
I2C_WriteReg(I2C1, 0x68 << 1, 0x6B, 0x00); 

// Burst read 6 bytes (Accel X, Y, Z)
uint8_t accelData[6];
I2C_Read_Burst(I2C1, 0x68 << 1, 0x3B, accelData, 6);
```

---

<h2 align=center>EXTI</h2>

| Functions | Description |
| --------- | ----- |
| void extiRegisterHandler(Pin_t pin, ExtiHandler_t handler) | Configures the external interrupt line for a specific pin and selects the trigger edge (Rising/Falling/Both). |
| void extiInit(Pin_t pin, ExtiEdge) | Registers a callback function to be executed when the interrupt occurs on the specified pin. |
| void extiClearFlag(Pin_t pin) | Clears the Pending Register (PR) bit to acknowledge the interrupt and allow future triggers. |
| void EXTIx_User_Handler(void) | Weak interrupt handlers. Can be overridden in user code to implement custom logic without modifying the library. |

## How to use
> [!NOTE]
>- **Mapping**: EXTI lines are shared between ports (e.g., PA0, PB0, and PC0 all use EXTI0). The library handles the AFIO multiplexer to route the correct port to the line.
>- **Handlers**: Use extiRegisterHandler to pass a function pointer. This keeps your main.c clean and modular.
>- **Pending Flags**: The extiClearFlag is called internally within the ISR (Interrupt Service Routine) to ensure the interrupt doesn't re-trigger immediately, but it is also available for manual control.

## Code Example: EXTI (Interrupts)
```C
// Callback function for button press
void myButtonHandler(void) {
    pinToggle(PC13); // Flash LED on press
}

// Setup EXTI on PA0, Falling edge trigger
extiInit(PA0, EXTI_FALLING);
extiRegisterHandler(PA0, myButtonHandler);
```

---

<h2 align=center>DMA</h2>

| Functions | Description |
| --------- | ----- |
| void DMAInit(DMA_Channel_TypeDef *DMAx, uint32_t peripheral_addr, uint32_t memory_addr, uint16_t data_count) | Configures the Direct Memory Access channel to transfer data between a peripheral and memory without CPU intervention. |

## How to use
> [!NOTE]
>- **Channel Mapping**: Each peripheral is tied to a specific DMA channel (e.g., USART1_TX is always DMA1 Channel 4 on STM32F103). Refer to the datasheet before initialization.
>- **Memory Alignment**: Ensure your source/destination buffers are properly aligned and volatile if they are modified in interrupts.
>- **CPU Efficiency**: Once started, the CPU is completely free to execute other tasks. Use DMA interrupt flags to detect when the transfer is complete.

## Code Example: DMA (Memory to Peripheral)
```C
// Example: Transferring a buffer to USART1 via DMA
uint8_t tx_buffer[] = "Hexapod Movement Data...";

DMAInit(DMA1_Channel4, (uint32_t)&USART1->DR, (uint32_t)tx_buffer, sizeof(tx_buffer));

// Start the transfer (depends on your implementation)
DMA1_Channel4->CCR |= DMA_CCR_EN;
```

---

<h2 align=center>ADC</h2>

| Functions | Description |
| --------- | ----- |
| void ADCInitMulti(Pin_t *pins, uint16_t count, uint8_t need_dma) | Initializes multiple ADC channels for a given array of pins. Includes optional DMA support for autonomous data collection from multiple sensors. |

## How to use
> [!NOTE]
>- **Scan Mode**: ADCInitMulti configures the ADC in scan mode, where it cycles through all specified channels automatically.
>- **DMA Integration**: When need_dma is enabled, the ADC triggers a DMA request after each conversion, moving the result directly into your memory buffer (sensorValues).
>- **Calibration**: It is recommended to run the internal ADC calibration (RSTCAL/CAL bits) during initialization to ensure maximum accuracy for your sensors.

## Code Example: ADC with DMA
```C
// Define pins for tension sensors (potentiometers/strain gauges)
Pin_t sensors[] = {PA0, PA1, PA2};
uint16_t sensorValues[3];

// Initialize 3 channels with DMA auto-transfer to our array
ADCInitMulti(sensors, 3, 1);

// Data in 'sensorValues' updates automatically in the background
while(1) {
    if (sensorValues[0] > 2000) {
        // High tension detected on Leg 1!
    }
}
```

---

<h2 align=center>SOFT UART</h2>

| Functions | Description |
| --------- | ----- |
| uint16_t softUartAvailable(void) | Returns the number of received bytes currently stored in the software buffer. Non-blocking check. |
| char softUartReadByte(void) | Retrieves one byte from the software receive buffer. |
| void softUartPutChar(char data) | Transmits a single character by manually toggling (bit-banging) the TX pin with precise timing. |
| void softUartPutString(const char *data) | Sends a null-terminated string using the bit-banging transmission method. |
| void softUartInit(Pin_t rx, Pin_t tx, uint32_t baud_rate) | Configures GPIO pins for RX/TX and sets up a timer to generate bit-interval interrupts for the specified baud rate. |

## Hardware Constraints & Shared Access
> [!IMPORTANT]
> **Timer Resource Sharing:** > * **Occupied:** `TIM3` (Base) and `Channel 1` (dedicated to Soft UART bit-timing).
> * **Available:** `TIM3` Channels 2, 3, and 4 can still be used for **PWM**.
> * **Constraint:** All PWM signals on these channels will share the same frequency as the Soft UART sampling rate. Adjusting `ARR` or `PSC` for PWM will break the UART communication.

## How to use
> [!NOTE]
>- **Timer Dependency**: Soft UART relies on a hardware timer to maintain precise bit-timing. Ensure the timer used by the library doesn't conflict with your PWM or millis() timers.
>- **Baud Rate Limits**: Since this is "bit-banging", high baud rates (above 38400) may significantly increase CPU load due to high interrupt frequency. Best used for 9600 or 19200.
>- **Interrupt Priority**: For stable reception, the Soft UART timer interrupt should have a high priority to avoid bit-drifting during heavy processing.

## Code Example: Soft UART
```C
// Initialize Soft UART on PB0 (RX) and PB1 (TX) at 9600 baud
softUartInit(PB0, PB1, 9600);

// Sending status from the glove
softUartPutString("Glove MK2: Fingers calibrated.");

// Receiving data
if (softUartAvailable()) {
    char cmd = softUartReadByte();
    // Handle command...
}
```

---

<h2 align=center>SOFT I2C</h2>

| Functions | Description |
| --------- | ----- |
| void SoftI2C_Start(Pin_t sda, Pin_t scl) | Generates a START condition by pulling the SDA line low while SCL is high. Signals the beginning of a data transfer. |
| void SoftI2C_Stop(Pin_t sda, Pin_t scl) | Generates a STOP condition by releasing the SDA line to high while SCL is high. Ends the current bus session. |
| uint8_t SoftI2C_Write(Pin_t sda, Pin_t scl, uint8_t byte) | Transmits an 8-bit byte to the bus bit-by-bit. Returns the ACK/NACK bit received from the slave device. |
| uint8_t SoftI2C_Read(Pin_t sda, Pin_t scl, uint8_t ack) | Receives an 8-bit byte from the slave and sends an ACK or NACK bit to control the data flow. |

## How to use
> [!NOTE]
>- **Pin Configuration**: Ensure the pins are configured as Open-Drain. Soft I2C relies on external or internal pull-up resistors.
>- **Timing**: This implementation uses software delays to match the I2C Standard Mode (~100kHz). CPU frequency affects the bus speed.
>- **Flexibility**: Unlike hardware I2C, Soft I2C can be initialized on any two GPIO pins, making it ideal for prototypes with complex routing.

## Code Example: Soft I2C
```C
// Reading WHO_AM_I using bit-banging
SoftI2C_Start(PA11, PA12);
SoftI2C_Write(PA11, PA12, 0x68 << 1); // Device Address + W
SoftI2C_Write(PA11, PA12, 0x75);      // Register Address
SoftI2C_Start(PA11, PA12);            // Restart
SoftI2C_Write(PA11, PA12, (0x68 << 1) | 1); // Device Address + R
uint8_t id = SoftI2C_Read(PA11, PA12, 0);   // Read with NACK
SoftI2C_Stop(PA11, PA12);
```
