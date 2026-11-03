/**
 * @file lcd_driver.c
 *
 * @brief Driver for the 16x2 LCD in 4-bit mode, through the PCF8574 I2C
 *        backpack. i2c_master_init() has to be called first.
 *
 * @date 10/07/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>

 #include <i2c.h>
 #include <lcd_driver.h>
 #include <systick.h>
 
 /** @brief PCF8574 address with A0-A2 high (0x4E >> 1) */
 #define LCD_ADDR 0x27
 
 /** @brief Control bits on P0-P3 of the PCF8574 (RW on P1 stays 0) */
 #define LCD_RS        (1 << 0)
 #define LCD_EN        (1 << 2)
 #define LCD_BACKLIGHT (1 << 3)
 
 /** @brief RS = 0 for instructions, RS = 1 for characters */
 #define LCD_CMD  0
 #define LCD_CHAR LCD_RS
 
 /** @brief Data bits are the upper 4 bits (P4-P7 -> D4-D7) */
 #define UPPER_NIBBLE 0xF0
 #define NIBBLE_SHIFT 4
 
 /** @brief Bytes per I2C write: 4 for a full byte, 2 for a single half */
 #define LCD_BYTE_LEN   4
 #define LCD_NIBBLE_LEN 2
 
 /** @brief HD44780 instructions (p. 24) */
 #define LCD_CLEAR        0x01
 #define LCD_ENTRY_MODE   0x06  /* cursor moves right */
 #define LCD_DISPLAY_ON   0x0F  /* display, cursor and blink on */
 #define LCD_FUNCTION_SET 0x28  /* 4-bit, 2 lines, 5x8 font */
 #define LCD_SET_DDRAM    0x80
 
 /** @brief Row 1 starts at DDRAM address 0x40 (p. 11) */
 #define LCD_ROW_OFFSET 0x40
 
 /** @brief First four init steps from figure 24 (p. 46) */
 #define LCD_INIT_8BIT 0x3
 #define LCD_INIT_4BIT 0x2
 
 /** @brief Busy-wait loop counts */
 #define POWER_ON_DELAY 41  /* LCD needs > 40 ms after power on */
 #define INIT_DELAY     5   /* covers the 4.1 ms init wait */
 #define CLEAR_DELAY    2   /* covers the 2mS clear delay */
 
 /**
  * @brief Busy-waits for count loop iterations.
  
 static void lcd_delay(int count) {
     for (volatile int i = 0; i < count; i++) {
     }
 }
     */
 
 /**
  * @brief Sends an 8-bit instruction or character as 4 bytes in one write:
  *        upper half with E = 1, then E = 0, then the lower half the same way.
  *
  * @param value  instruction or character
  * @param rs     LCD_CMD or LCD_CHAR
  */
 static void lcd_send(uint8_t value, uint8_t rs) {
     uint8_t buf[LCD_BYTE_LEN];
     uint8_t upper = value & UPPER_NIBBLE;
     uint8_t lower = (value << NIBBLE_SHIFT) & UPPER_NIBBLE;
 
     buf[0] = upper | LCD_BACKLIGHT | LCD_EN | rs;
     buf[1] = upper | LCD_BACKLIGHT | rs;
     buf[2] = lower | LCD_BACKLIGHT | LCD_EN | rs;
     buf[3] = lower | LCD_BACKLIGHT | rs;
 
     i2c_master_write(buf, LCD_BYTE_LEN, LCD_ADDR);
 }
 
 /**
  * @brief Sends just one half of an instruction. Only used for the first
  *        init steps, while the LCD is still in 8-bit mode and each E pulse
  *        counts as a whole instruction.
  */
 static void lcd_send_nibble(uint8_t nibble) {
     uint8_t buf[LCD_NIBBLE_LEN];
 
     buf[0] = (nibble << NIBBLE_SHIFT) | LCD_BACKLIGHT | LCD_EN;
     buf[1] = (nibble << NIBBLE_SHIFT) | LCD_BACKLIGHT;
 
     i2c_master_write(buf, LCD_NIBBLE_LEN, LCD_ADDR);
 }
 
 /**
  * @brief Puts the LCD in 4-bit, 2-line mode with a blinking cursor,
  *        then clears it.
  */
 void lcd_driver_init() {
     systick_delay(POWER_ON_DELAY);
 
     // still in 8-bit mode for these four
     lcd_send_nibble(LCD_INIT_8BIT);
     systick_delay(INIT_DELAY);
     lcd_send_nibble(LCD_INIT_8BIT);
     lcd_send_nibble(LCD_INIT_8BIT);
     lcd_send_nibble(LCD_INIT_4BIT);
 
     // now in 4-bit mode, so everything is sent as two halves
     lcd_send(LCD_FUNCTION_SET, LCD_CMD);
     lcd_send(LCD_DISPLAY_ON, LCD_CMD);
     lcd_send(LCD_ENTRY_MODE, LCD_CMD);
     systick_delay(INIT_DELAY);
     lcd_clear();
 }
 
 /**
  * @brief Prints a null-terminated string at the cursor.
  */
 void lcd_print(char *input) {
     while (*input != '\0') {
         lcd_send(*input, LCD_CHAR);
         input++;
     }
 }
 
 /**
  * @brief Moves the cursor.
  *
  * @param row  0 or 1
  * @param col  0 to 15
  */
 void lcd_set_cursor(uint8_t row, uint8_t col) {
     lcd_send(LCD_SET_DDRAM | (row * LCD_ROW_OFFSET + col), LCD_CMD);
 }
 
 /**
  * @brief Clears the screen and moves the cursor back to row 0, col 0.
  */
 void lcd_clear() {
     lcd_send(LCD_CLEAR, LCD_CMD);
     uint32_t start_ticks = systick_get_ticks();
     while ((systick_get_ticks() - start_ticks) < CLEAR_DELAY);
 }
 