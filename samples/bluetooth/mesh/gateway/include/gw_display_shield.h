#ifndef GW_DISPLAY_SHIELD_H__
#define GW_DISPLAY_SHIELD_H__

#include <zephyr.h>
#include "gw_display_mcp23017.h"

// commands
#define LCD_CLEARDISPLAY 0x01 //!< Clear display, set cursor position to zero
#define LCD_RETURNHOME 0x02 //!< Set cursor position to zero
#define LCD_ENTRYMODESET 0x04 //!< Sets the entry mode
#define LCD_DISPLAYCONTROL                                                     \
	0x08 //!< Controls the display; allows you to do stuff like turn it on and off
#define LCD_CURSORSHIFT 0x10 //!< Lets you move the cursor
#define LCD_FUNCTIONSET 0x20 //!< Used to send the function set to the display
#define LCD_SETCGRAMADDR                                                       \
	0x40 //!< Used to set the CGRAM (character generator RAM)
#define LCD_SETDDRAMADDR 0x80 //!< Used to set the DDRAM (display data RAM)

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00 //!< Used to set text to flow from right to left
#define LCD_ENTRYLEFT 0x02 //!< Used to set text to flow from left to right
#define LCD_ENTRYSHIFTINCREMENT                                                \
	0x01 //!< Used to 'right justify' text from the cursor
#define LCD_ENTRYSHIFTDECREMENT                                                \
	0x00 //!< USed to 'left justify' text from the cursor

// flags for display on/off control
#define LCD_DISPLAYON 0x04 //!< Turns the display on
#define LCD_DISPLAYOFF 0x00 //!< Turns the display off
#define LCD_CURSORON 0x02 //!< Turns the cursor on
#define LCD_CURSOROFF 0x00 //!< Turns the cursor off
#define LCD_BLINKON 0x01 //!< Turns on the blinking cursor
#define LCD_BLINKOFF 0x00 //!< Turns off the blinking cursor

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08 //!< Flag for moving the display
#define LCD_CURSORMOVE 0x00 //!< Flag for moving the cursor
#define LCD_MOVERIGHT 0x04 //!< Flag for moving right
#define LCD_MOVELEFT 0x00 //!< Flag for moving left

// flags for function set
#define LCD_8BITMODE 0x10 //!< LCD 8 bit mode
#define LCD_4BITMODE 0x00 //!< LCD 4 bit mode
#define LCD_2LINE 0x08 //!< LCD 2 line mode
#define LCD_1LINE 0x00 //!< LCD 1 line mode
#define LCD_5x10DOTS 0x04 //!< 10 pixel high font mode
#define LCD_5x8DOTS 0x00 //!< 8 pixel high font mode

#define BUTTON_UP 0x08 //!< Up button
#define BUTTON_DOWN 0x04 //!< Down button
#define BUTTON_LEFT 0x10 //!< Left button
#define BUTTON_RIGHT 0x02 //!< Right button
#define BUTTON_SELECT 0x01 //!< Select button

/*!
* @brief Init RGB LCD shield
*/
void display_shield_init(void);

/*!
* @brief Starts I2C connection with display
* @param cols Sets the number of columns
* @param rows Sets the number of rows
* @param charsize Sets the character size
* @return Returns true if the connection was successful
*/
void display_begin(uint8_t cols, uint8_t rows, uint8_t charsize);

/*!
* @brief High-level command to clear the display
*/
void display_clear(void);
/*!
* @brief High-level command to set the cursor position to zero
*/
void display_home(void);
/*!
* @brief High-level command to set the cursor position
* @param col Column
* @param row Row
*/
void display_set_cursor(uint8_t col, uint8_t row);
/*!
* @brief High-level command to turn the display off
*/
void display_off(void);
/*!
* @brief High-level command to turn the display on
*/
void display_on(void);
/*!
* @brief High-level command to turn off the blinking cursor off
*/
void display_blink_off(void);
/*!
* @brief High-level command to turn on the blinking cursor
*/
void display_blink_on(void);
/*!
* @brief High-level command to turn the underline cursor off
*/
void display_cursor_off(void);
/*!
* @brief High-level command to turn the underline cursor on
*/
void display_cursor_on(void);
/*!
* @brief High-level command to scroll display left without changing the RAM
*/
void display_scroll_display_left(void);
/*!
* @brief High-level command to scroll display right without changing the RAM
*/
void display_scroll_display_right(void);
/*!
* @brief High-level command to make text flow right to left
*/
void display_left_to_right(void);
/*!
* @brief High-level command to make text flow left to right
*/
void display_right_to_left(void);
/*!
* @brief High-level command to 'right justify' text from cursor
*/
void display_autoscroll(void);
/*!
* @brief High-level command to 'left justify' text from cursor
*/
void display_no_autoscroll(void);

/*!
* @brief High-level command to set the backlight, only if the LCD backpack is
* used
* @param status Status to set the backlight
*/
void display_set_backlight(uint8_t status);

/*!
* @brief High-level command that creates custom characters in CGRAM
* @param location Location in cgram to fill
* @param charmap[] Character map to use
*/
void display_create_char(uint8_t, uint8_t[]);

/*!
* @brief Mid-level command that sends data to the display
* @param value Data to send to the display
*/
void display_write(uint8_t value);

/*!
* @brief Sends command to display
* @param value Command to send
*/
void command(uint8_t);

/*!
* @brief reads the buttons from the shield
* @return Returns what buttons have been pressed
*/
uint8_t display_read_buttons(void);

#endif /* GW_DISPLAY_SHIELD_H__ */
