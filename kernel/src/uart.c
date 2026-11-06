/**
 * @file uart.c
 *
 * @brief UART implemented with interrupts. Intializes, gets bytes, and puts bytes.
 *
 * @date 10/09/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>
 #include <rcc.h>
 #include <uart.h>
 #include <uart_polling.h>
 #include <nvic.h>
 #include <gpio.h>
 #include <arm.h>
 
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
 
 #define BUF_SIZE 256
 
 //struct for circular array buffers rx_buf anf tx-buf
 //head and tail for indexes, num for number of current items
 struct uart_buf_t {
   char data[BUF_SIZE];
   uint32_t head;
   uint32_t tail;
   volatile uint32_t num;
 };
 
 static struct uart_buf_t tx_storage;
 static struct uart_buf_t rx_storage;
 
 static struct uart_buf_t *tx_buf = &tx_storage;
 static struct uart_buf_t *rx_buf = &rx_storage;
 
 /** @brief Base address for UART2 */
 #define UART2_BASE  (struct uart_reg_map *) 0x40004400
 
 #define UNUSED __attribute__((unused))
 
 /** @brief Enable  Bit for UART Config register */
 #define UART_EN (1 << 13)
 
 /** @brief Enable  Bit for RCC Config register */
 #define RCC_EN (1 << 17)
 
 /** @brief Enable  Bit for UART Transmission */
 #define TRANS_EN (1 << 3)
 
 /** @brief Enable  Bit for UART Recieving */
 #define REC_EN (1 << 2)
 
 /** @brief IRQ number for UART1 */
 #define IRQ_UART2 38
 
 /** @brief UART enable bit for TX w/ interrupts - pg 547 */
 #define TXEIE_EN 1 << 7
 
 /** @brief UART enable bit for RX w/ interrupts */
 #define RXNEIE_EN 1 << 5
 
 /** @brief Bit to check for UART DR empty */
 #define TXE_Bit (1 << 7)
 
 /** @brief Bit to check for UART DR ready to read */
 #define RXNE_Bit (1 << 5)
 
 /** @brief Bit mask for DR reading */
 #define DR_READ_MASK 0xFF
 
 // !!!!!! Never do |= or &= to the data register! We will deduct points.
 
 static void buf_init(struct uart_buf_t *buf){
 //  buf = (struct uart_buf_t *) sizeof(struct uart_buf_t);
   buf->head = 0;
   buf->tail = 0;
   buf->num = 0;
 }
 
 /**
  * @brief initializes UART to given baud rate with 8-bit word length, 1 stop bit, 0 parity bits
  *         Added compatibility/enable for interrupts
  *
  * @param baud USARTDIV value for the desired baud rate
  */
 void uart_init(int baud){
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
   uart->CR1 &= ~(TXEIE_EN);
   uart->CR1 |= RXNEIE_EN;
 
   nvic_irq(IRQ_UART2, IRQ_ENABLE);
 
   //init the buffers! rx and tx
   buf_init(tx_buf);
   buf_init(rx_buf);
 }
 
 /**
  * @brief queue data onto specified buffer, check buffer is full
  *
  * @param char c queue onto to uart_buf_t *buf
  *  0 when success, -1 when full
  */
 static int buf_queue(struct uart_buf_t *buf, char c){
   if (buf->num == BUF_SIZE) return -1;
   buf->data[buf->head] = c;
 //  buf->head = (buf->head + 1) % BUF_SIZE;
   buf->head++;
   if (buf->head == BUF_SIZE) buf->head = 0;
   buf->num++;
   return 0;
 }
 
 /**
  * @brief dequeue data from specified buffer, check buffer is empty
  *
  * @param char *c dequeued from uart_buf_t *buf
  *  0 when success, -1 when empty
  */
 static int buf_dequeue(struct uart_buf_t *buf, char *c){
   if (buf->num == 0) return -1;
   *c = buf->data[buf->tail];
 //  buf->tail = (buf->tail + 1) % BUF_SIZE;
   buf->tail++;
   if (buf->tail == BUF_SIZE) buf->tail = 0;
   buf->num--;
   return 0;
 }
 
 /**
  * @brief places char c into kernel buffer
  *        blocks IRQ changing buffer 
  *
  * @param char c to be added to kernel buffer
  */
 int uart_put_byte(char c){
   struct uart_reg_map *uart = UART2_BASE;
   int interruptState = save_interrupt_state_and_disable();
   uart->CR1 &= ~(TXEIE_EN);
   int q_status = buf_queue(tx_buf, c);
   uart->CR1 |= TXEIE_EN;
   restore_interrupt_state(interruptState);
   return q_status;
 }
 
 /**
  * @brief Takes a BYTE out of the kernel buffer - 0 on success -1 on failure
  *
  * @param char *c for byte of removed data to be assigned to
  */
 int uart_get_byte(char *c){
   struct uart_reg_map *uart = UART2_BASE;
   int interruptState = save_interrupt_state_and_disable();
   uart->CR1 &= ~(RXNEIE_EN);
   int q_status = buf_dequeue(rx_buf, c);
   uart->CR1 |= RXNEIE_EN;
   restore_interrupt_state(interruptState);
   return q_status;
 }
 
 /**
  * @brief Moves a BYTE from TX kernel buffer to TX data reg
  */
 void USART2_TX_IRQHandler(){
   struct uart_reg_map *uart = UART2_BASE;
   char c;
   int q_status = buf_dequeue(tx_buf, &c);
   if (q_status == -1){
     uart->CR1 &= ~(TXEIE_EN);
     return;
   }
   uart->DR = (uint32_t)c;
   if (tx_buf->num == 0) uart->CR1 &= ~(TXEIE_EN);
 };
 
 /**
  * @brief Moves a BYTE from RX data reg to RX kernel buffer
  */
 void USART2_RX_IRQHandler(){
   struct uart_reg_map *uart = UART2_BASE;
   char c = (char) (uart->DR & DR_READ_MASK);
   int q_status = buf_queue(rx_buf, c);
 //  if (q_status == -1)
   if ((q_status == -1) || (rx_buf->num == BUF_SIZE))
     uart->CR1 &= ~(RXNEIE_EN);
 };
 
 /**
  * @brief Checks whether to use RX handler or TX handler
  *        Clears nvic pending bit
  */
 void USART2_IRQHandler(){
   struct uart_reg_map *uart = UART2_BASE;
   uint32_t status = uart->SR;
   uint32_t control = uart->CR1;
 //  if ((TXE_Bit != 0) && (TXEIE_EN != 0))
   if ((status & TXE_Bit) && (control & TXEIE_EN))
     USART2_TX_IRQHandler();
 //  if ((RXNE_Bit != 0) && (RXNEIE_EN != 0))
   if ((status & RXNE_Bit) && (control & RXNEIE_EN))
     USART2_RX_IRQHandler();
   nvic_clear_pending(IRQ_UART2);
 }
 
 /**
  * @brief Flushes UART buffers tx_buf and rx_buf
  */
 void uart_flush(){
   struct uart_reg_map *uart = UART2_BASE;
   char c;
   uart->CR1 &= ~(TXEIE_EN);
   uart->CR1 &= ~(RXNEIE_EN);
   while (buf_dequeue(tx_buf, &c) == 0){
     uart_polling_put_byte(c);
   }
   while (buf_dequeue(rx_buf, &c) == 0);
 }