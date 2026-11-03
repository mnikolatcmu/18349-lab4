/**
 * @file i2c.c
 *
 * @brief Polling I2C master driver. Writes to LCD backpack.
 *
 * @date 10/07/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>

 #include <gpio.h>
 #include <i2c.h>
 #include <rcc.h>
 
 /** @brief I2C register map */
 struct i2c_reg_map {
     volatile uint32_t CR1;    /**< 0x00 */
     volatile uint32_t CR2;    /**< 0x04 */
     volatile uint32_t OAR1;   /**< 0x08 */
     volatile uint32_t OAR2;   /**< 0x0C */
     volatile uint32_t DR;     /**< 0x10 */
     volatile uint32_t SR1;    /**< 0x14 */
     volatile uint32_t SR2;    /**< 0x18 */
     volatile uint32_t CCR;    /**< 0x1C */
     volatile uint32_t TRISE;  /**< 0x20 */
     volatile uint32_t FLTR;   /**< 0x24 */
 };
 
 /** @brief Base address of I2C1 */
 #define I2C1_BASE (struct i2c_reg_map *) 0x40005400
 
 /** @brief GPIO port and pin for SCL (D15) */
 #define SCL_PORT GPIO_B
 #define SCL_PIN  8
 
 /** @brief GPIO port and pin for SDA (D14) */
 #define SDA_PORT GPIO_B
 #define SDA_PIN  9
 
 /** @brief I2C1 clock enable bit in RCC_APB1ENR */
 #define I2C1_EN (1 << 21)
 
 /** @brief APB1 clock is 16 MHz */
 #define PCLK_MHZ 16
 
 /** @brief max rise time: 1000 ns / 62.5 ns + 1 */
 #define MAX_RISE_TIME 17
 
 /** @brief CR1 bits */
 #define I2C_PE    (1 << 0)
 #define I2C_START (1 << 8)
 #define I2C_STOP  (1 << 9)
 
 /** @brief SR1 bits */
 #define I2C_SR1_SB   (1 << 0)
 #define I2C_SR1_ADDR (1 << 1)
 #define I2C_SR1_BTF  (1 << 2)
 #define I2C_SR1_TXE  (1 << 7)
 
 /** @brief last bit of the address byte, 0 means write */
 #define I2C_WRITE 0
 
 /**
  * @brief Sets up I2C1 on D14/D15 as a standard mode master.
  *
  * @param clk  SCL frequency in kHz
  */
 void i2c_master_init(uint16_t clk) {
     struct rcc_reg_map *rcc = RCC_BASE;
     struct i2c_reg_map *i2c = I2C1_BASE;
 
     gpio_init(SCL_PORT, SCL_PIN, MODE_ALT, OUTPUT_OPEN_DRAIN,
               OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);
     gpio_init(SDA_PORT, SDA_PIN, MODE_ALT, OUTPUT_OPEN_DRAIN,
               OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);
 
     rcc->apb1_enr |= I2C1_EN;
 
     i2c->CR2 |= PCLK_MHZ;
     i2c->CCR |= (PCLK_MHZ * 1000) / (2 * clk);
     i2c->TRISE = MAX_RISE_TIME;
     i2c->CR1 |= I2C_PE;
 }
 
 /**
  * @brief Sends a start condition and waits for SB (EV5).
  */
 void i2c_master_start() {
     struct i2c_reg_map *i2c = I2C1_BASE;
 
     i2c->CR1 |= I2C_START;
     while (!(i2c->SR1 & I2C_SR1_SB)) {
     }
 }
 
 /**
  * @brief Sends a stop condition and waits until it's done.
  */
 void i2c_master_stop() {
     struct i2c_reg_map *i2c = I2C1_BASE;
 
     i2c->CR1 |= I2C_STOP;
     while (i2c->CR1 & I2C_STOP) {
     }
 }
 
 /**
  * @brief Sends len bytes to a slave in one start/stop transmission,
  *        following the master transmitter steps.
  *
  * @param buf         bytes to send
  * @param len         number of bytes
  * @param slave_addr  7-bit slave address
  *
  * @return 0 when done
  */
 int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr) {
     struct i2c_reg_map *i2c = I2C1_BASE;
     uint32_t temp;
 
     i2c_master_start();
     i2c->DR = (slave_addr << 1) | I2C_WRITE;
 
     while (!(i2c->SR1 & I2C_SR1_ADDR)) {
     }
     temp = i2c->SR2;
     (void) temp;
 
     for (int i = 0; i < len; i++) {
         while (!(i2c->SR1 & I2C_SR1_TXE)) {
         }
         i2c->DR = buf[i];
     }
 
     while (!(i2c->SR1 & I2C_SR1_TXE) || !(i2c->SR1 & I2C_SR1_BTF)) {
     }
 
     i2c_master_stop();
     return 0;
 }
 
 int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr) {
     (void) buf;
     (void) len;
     (void) slave_addr;
 
     return 0;
 }
 