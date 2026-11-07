/**
 * @file uart_polling.c
 *
 * @brief Polling to UART. Intializes, gets bytes, and puts bytes.
 *
 * @date 10/07/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */


 #include <unistd.h>

 #include <gpio.h>
 #include <rcc.h>
 #include <uart_polling.h>
 
 /** @brief The UART register map. */
 struct uart_reg_map {
     volatile uint32_t SR;   /**< Status Register */
     volatile uint32_t DR;   /**<  Data Register */
     volatile uint32_t BRR;  /**<  Baud Rate Register */
     volatile uint32_t CR1;  /**<  Control Register 1 */
     volatile uint32_t CR2;  /**<  Control Register 2 */
     volatile uint32_t CR3;  /**<  Control Register 3 */
     volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
 };
 
 /** @brief Base address for UART2 */
 #define UART2_BASE  (struct uart_reg_map *) 0x40004400
 
 /** @brief Enable  Bit for UART Config register */
 #define UART_EN (1 << 13)
 
 /** @brief Enable  Bit for RCC Config register */
 #define RCC_EN (1 << 17)
 
 /** @brief Enable  Bit for UART Transmission */
 #define TRANS_EN (1 << 3)
 
 /** @brief Enable  Bit for UART Recieving */
 #define REC_EN (1 << 2)
 
 /** @brief Bit to check for UART DR empty */
 #define TXE_Bit (1 << 7)
 
 /** @brief Bit to check for UART DR ready to read */
 #define RXNE_Bit (1 << 5)
 
 
 /**
  * @brief initializes UART to given baud rate with 8-bit word length, 1 stop bit, 0 parity bits
  *
  * @param baud USARTDIV value for the desired baud rate
  */
 void uart_polling_init (int baud){
     //TX
     gpio_init(GPIO_A, 2, MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);
     //RX
     gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);
 
     //enable the uart clock before touching any uart register
     struct rcc_reg_map *rcc = RCC_BASE;
     rcc->apb1_enr |= RCC_EN;
 
     struct uart_reg_map *uart = UART2_BASE;
 
     //baud rate/usart div, baud should always be USARTDIV
     uart->BRR = baud;
 
     //enable transmitter and reciever, then the uart itself
     uart->CR1 |= TRANS_EN;
     uart->CR1 |= REC_EN;
     uart->CR1 |= UART_EN;
     return;
 }
 
 /**
  * @brief transmits a byte over UART
  *
  * @param c character to be sent
  */
 void uart_polling_put_byte (char c){
     struct uart_reg_map *uart = UART2_BASE;
     //while data is still in DR
     while ((uart->SR & TXE_Bit) == 0){
         //waiting waiting waiting
     }
     uart->DR = c;
     return;
 }
 
 /**
  * @brief receives a byte over UART
  */
 char uart_polling_get_byte () {
     struct uart_reg_map *uart = UART2_BASE;
     //while data not ready to be read
     while ((uart->SR & RXNE_Bit) == 0){
         //waitinbg
     }
     return uart->DR;
 }