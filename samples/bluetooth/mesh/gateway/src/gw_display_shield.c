/*!
 * @file Adafruit_RGBLCDShield.cpp
 *
 * @mainpage Adafruit RGB LCD Shield Library
 *
 * @section intro_sec Introduction
 *
 * This is a library for the Adafruit RGB 16x2 LCD Shield
 * Pick one up at the Adafruit shop!
 * ---------> http://http://www.adafruit.com/products/714
 *
 * The shield uses I2C to communicate, 2 pins are required to
 * interface
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * @section author Author
 *
 * Written by Limor Fried/Ladyada for Adafruit Industries.
 * @section license License
 *
 * BSD license, all text above must be included in any redistribution
 */

#include <zephyr.h>
#include <stdio.h>
#include "gw_display_shield.h"

static uint8_t _data_pins[4] = { 12, 11, 10, 9 };
static uint8_t _button_pins[5] = { 0, 1, 2, 3, 4 };
static uint8_t _enable_pin = 13; // activated by a HIGH pulse
static uint8_t _rw_pin = 14; // LOW: write to LCD  HIGH: read from LCD
static uint8_t _rs_pin = 15; // LOW: command  HIGH: character
static uint8_t _displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
static uint8_t _displaycontrol;
static uint8_t _displaymode;

static uint8_t _numlines = 2;

static void write4bits(uint8_t value)
{
	uint16_t out = 0;

	out = mcp23017_read_gpio_ab();

	/* Speed up for i2c since its sluggish */
	for (int i = 0; i < 4; i++) {
		out &= ~(1 << _data_pins[i]);
		out |= ((value >> i) & 0x1) << _data_pins[i];
	}

	/* Make sure enable is low */
	out &= ~(1 << _enable_pin);

	mcp23017_write_gpio_ab(out);

	/* Pulse enable */
	k_sleep(K_USEC(1));
	out |= (1 << _enable_pin);
	mcp23017_write_gpio_ab(out);
	k_sleep(K_USEC(1));
	out &= ~(1 << _enable_pin);
	mcp23017_write_gpio_ab(out);
	k_sleep(K_USEC(100));
}

static void send(uint8_t value, uint8_t mode)
{
	// mcp23017_write_pin(_rs_pin, mode); // *** THE PROBLEM IS THAT THIS NEVER GOES HIGH ***

	/* Hack */
	if (mode == LOW) {
		mcp23017_pin_configure(_rs_pin, OUTPUT);
	} else {
		mcp23017_pin_configure(_rs_pin, INPUT);
	}
	/* **** */

	mcp23017_write_pin(_rw_pin, LOW);

	write4bits(value >> 4);
	write4bits(value);
}

void display_shield_init(void)
{
	mcp23017_init();

	/* Controlling the 3 backlight RBG LEDs */
	mcp23017_pin_configure(8, OUTPUT);
	mcp23017_pin_configure(6, OUTPUT);
	mcp23017_pin_configure(7, OUTPUT);
	// mcp23017_write_pin(8, HIGH);
	mcp23017_write_pin(6, LOW);
	mcp23017_write_pin(7, LOW);
	// display_set_backlight(0x0); // NO NOT WORK PROPERLY FOR PIN B (8->)

	/* Configuring display controlling/writing pins */
	mcp23017_pin_configure(_rw_pin, OUTPUT);
	mcp23017_pin_configure(_rs_pin, OUTPUT);
	mcp23017_pin_configure(_enable_pin, OUTPUT);
	for (uint8_t i = 0; i < 4; i++) {
		mcp23017_pin_configure(_data_pins[i], OUTPUT);
	}

	/* Configuring button pins */
	for (uint8_t i = 0; i < 5; i++) {
		mcp23017_pin_configure(_button_pins[i], INPUT);
		mcp23017_pull_up(_button_pins[i], HIGH);
	}

	/* SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	 * according to datasheet, we need at least 40ms after power rises above 2.7V
	 * before sending commands. Arduino can turn on way befer 4.5V so we'll wait
	 * 50
	 */
	k_sleep(K_USEC(50000));
	/* Pull both RS and R/W low to begin commands */
	// mcp23017_write_pin(_rs_pin, LOW); // Does not work properly
	mcp23017_write_pin(_enable_pin, LOW); // May not work properly
	mcp23017_write_pin(_rw_pin, LOW); // May not work properly

	/* Put the LCD into 4 bit according to the hitachi HD44780 datasheet figure 24, pg 46 */

	/* Start in 8bit mode, try to set 4 bit mode */
	write4bits(0x03);
	k_sleep(K_USEC(4500)); // wait min 4.1ms

	/* Second try */
	write4bits(0x03);
	k_sleep(K_USEC(4500)); // wait min 4.1ms

	/* Third go */
	write4bits(0x03);
	k_sleep(K_USEC(150));

	/* Finally, set to 8-bit interface */
	write4bits(0x02);

	/* Set # lines, font size, etc. */
	command(LCD_FUNCTIONSET | _displayfunction);

	/* Turn the display on with no cursor or blinking default */
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display_on();

	/* Clear it off */
	display_clear();

	/* Initialize to default text direction (for romance languages) */
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	/* Set the entry mode */
	command(LCD_ENTRYMODESET | _displaymode);
}

void display_clear(void)
{
	command(LCD_CLEARDISPLAY);
	/* Clearing display takes long time */
	k_sleep(K_USEC(2000));
}

void display_home(void)
{
	command(LCD_RETURNHOME);
	/* Returning home takes long time */
	k_sleep(K_USEC(2000));
}

void display_set_cursor(uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (row > _numlines) {
		row = _numlines - 1;
	}

	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void display_off(void)
{
	_displaycontrol &= ~LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_on(void)
{
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void display_cursor_off(void)
{
	_displaycontrol &= ~LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_cursor_on(void)
{
	_displaycontrol |= LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

/* Turn on and off blinking cursor */
void display_blink_off(void)
{
	_displaycontrol &= ~LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_blink_on(void)
{
	_displaycontrol |= LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

/* These commands scroll the display without changing the RAM */
void display_scroll_display_left(void)
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void display_scroll_display_right(void)
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

/* This is for text that flows Left to Right */
void display_left_to_right(void)
{
	_displaymode |= LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

/* This is for text that flows Right to Left */
void display_right_to_left(void)
{
	_displaymode &= ~LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

/* This will 'right justify' text from the cursor */
void display_autoscroll(void)
{
	_displaymode |= LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

/* This will 'left justify' text from the cursor */
void display_no_autoscroll(void)
{
	_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

/* Fill the first 8 CGRAM locations with custom characters */
void display_create_char(uint8_t location, uint8_t charmap[])
{
	location &= 0x7; // we only have 8 locations 0-7
	command(LCD_SETCGRAMADDR | (location << 3));
	for (int i = 0; i < 8; i++) {
		display_write(charmap[i]);
	}
	/* Unfortunately resets the location to 0,0 */
	command(LCD_SETDDRAMADDR);
}

/* Write a character to the display */
void display_write_char(char character)
{
	display_write(character);
}

/* Write a string to the display */
void display_write_string(const char *str)
{
	unsigned char i = 0;

	while (str[i] != '\0') {
		display_write(str[i++]);
	}
}

/* Uses potentially 3 character slots */
void display_write_number(uint16_t num)
{
	if (num > 999) {
		printk("Number larger than 3 digits\n");
		return;
	}

	char string[3];

	sprintf(string, "%d", num);

	display_write_char(string[0]);

	if (num > 9) {
		display_write_char(string[1]);
	}

	if (num > 99) {
		display_write_char(string[2]);
	}
}

void command(uint8_t value)
{
	send(value, LOW);
}

void display_write(uint8_t value)
{
	send(value, HIGH);
}

/* Allows to set the backlight, if the LCD backpack is used */
void display_set_backlight(uint8_t status)
{
	mcp23017_write_pin(
		8, ~(status >>
		     2) & 0x1); // ******** THIS DOES NOT WORK PROPERLY ********
	mcp23017_write_pin(7, ~(status >> 1) & 0x1);
	mcp23017_write_pin(6, ~status & 0x1);
}

uint8_t display_read_buttons(void)
{
	uint8_t ret;
	uint8_t reply = 0;

	for (uint8_t i = 0; i < 5; i++) {
		mcp23017_read_pin(_button_pins[i], &ret);
		reply |= (ret << i);
		reply ^= (1 << i);
	}

	return reply;
}
