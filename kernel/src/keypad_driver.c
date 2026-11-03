/**
 * @file keypad_driver.c
 *
 * @brief Driver for the 12-key keypad. Rows are outputs driven high one at
 *        a time, columns are inputs with pull-downs.
 *
 * @date 10/07/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>

 #include <gpio.h>
 #include <keypad_driver.h>
 #include <systick.h>
 
 /** @brief GPIO port and pin for col 1 (D8) */
 #define KEYPAD_COL1_PORT GPIO_A
 #define KEYPAD_COL1_PIN  9
 
 /** @brief GPIO port and pin for col 2 (D9) */
 #define KEYPAD_COL2_PORT GPIO_C
 #define KEYPAD_COL2_PIN  7
 
 /** @brief GPIO port and pin for col 3 (D10) */
 #define KEYPAD_COL3_PORT GPIO_B
 #define KEYPAD_COL3_PIN  6
 
 /** @brief GPIO port and pin for row 1 (D11) */
 #define KEYPAD_ROW1_PORT GPIO_A
 #define KEYPAD_ROW1_PIN  7
 
 /** @brief GPIO port and pin for row 2 (D12) */
 #define KEYPAD_ROW2_PORT GPIO_A
 #define KEYPAD_ROW2_PIN  6
 
 /** @brief GPIO port and pin for row 3 (D13) */
 #define KEYPAD_ROW3_PORT GPIO_A
 #define KEYPAD_ROW3_PIN  5
 
 /** @brief GPIO port and pin for row 4 (A4) */
 #define KEYPAD_ROW4_PORT GPIO_C
 #define KEYPAD_ROW4_PIN  1
 
 /** @brief Keypad size */
 #define NUM_ROWS 4
 #define NUM_COLS 3
 
 /** @brief Returned when there's no new key press */
 #define NO_KEY '\0'
 
 /** @brief Delay in ms to prevent debounce */
 #define DEBOUNCE_DELAY 20
 
 /** @brief Row and column pins, in order */
 static const gpio_port row_ports[NUM_ROWS] = {KEYPAD_ROW1_PORT, KEYPAD_ROW2_PORT,
                                               KEYPAD_ROW3_PORT, KEYPAD_ROW4_PORT};
 static const unsigned int row_pins[NUM_ROWS] = {KEYPAD_ROW1_PIN, KEYPAD_ROW2_PIN,
                                                 KEYPAD_ROW3_PIN, KEYPAD_ROW4_PIN};
 static const gpio_port col_ports[NUM_COLS] = {KEYPAD_COL1_PORT, KEYPAD_COL2_PORT,
                                               KEYPAD_COL3_PORT};
 static const unsigned int col_pins[NUM_COLS] = {KEYPAD_COL1_PIN, KEYPAD_COL2_PIN,
                                                 KEYPAD_COL3_PIN};
 
 /** @brief Character for each key, [row][col] */
 static const char keys[NUM_ROWS][NUM_COLS] = {
     {'1', '2', '3'},
     {'4', '5', '6'},
     {'7', '8', '9'},
     {'*', '0', '#'},
 };
 
 /**
  * @brief Sets the columns as pulled-down inputs and the rows as outputs.
  */
 void keypad_init() {
     for (int i = 0; i < NUM_COLS; i++) {
         gpio_init(col_ports[i], col_pins[i], MODE_INPUT, OUTPUT_PUSH_PULL,
                   OUTPUT_SPEED_LOW, PUPD_PULL_DOWN, ALT0);
     }
 
     for (int i = 0; i < NUM_ROWS; i++) {
         gpio_init(row_ports[i], row_pins[i], MODE_GP_OUTPUT, OUTPUT_PUSH_PULL,
                   OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
     }
 }
 
 /**
  * @brief Checks every row once.
  *
  * @return The first key held down, or NO_KEY
  */
 static char scan_keypad() {
     for (int row = 0; row < NUM_ROWS; row++) {
         gpio_set(row_ports[row], row_pins[row]);
 
         for (int col = 0; col < NUM_COLS; col++) {
             if (gpio_read(col_ports[col], col_pins[col])) {
                 gpio_clr(row_ports[row], row_pins[row]);
                 return keys[row][col];
             }
         }
 
         gpio_clr(row_ports[row], row_pins[row]);
     }
 
     return NO_KEY;
 }
 
 /**
  * @brief Returns a key everytime it's pressed.
  *
  * @return the key that was just pressed, or NO_KEY
  */
 char keypad_read() {
     static char last_key = NO_KEY;
     char key = scan_keypad();
 
     if (key == last_key) {
         return NO_KEY;
     }
     uint32_t start_ticks = systick_get_ticks();
     while ((systick_get_ticks() - start_ticks) < DEBOUNCE_DELAY);
     if (scan_keypad() != key) {
         return NO_KEY;
     }
 
     last_key = key;
     return key;
 }