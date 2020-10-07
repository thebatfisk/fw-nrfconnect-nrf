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
#include <inttypes.h> // TODO: Remove?
#include <stdio.h> // TODO: Remove?
#include <string.h> // TODO: Remove?
#include "gw_display_shield.h"

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//    DL = 1; 8-bit interface data
//    N = 0; 1-line display
//    F = 0; 5x8 dot character font
// 3. Display on/off control:
//    D = 0; Display off
//    C = 0; Cursor off
//    B = 0; Blinking off
// 4. Entry mode set:
//    I/D = 1; Increment by 1
//    S = 0; No shift
//

static uint8_t _rs_pin = 15; // LOW: command.  HIGH: character.
static uint8_t _rw_pin = 14; // LOW: write to LCD.  HIGH: read from LCD.
static uint8_t _enable_pin = 13; // activated by a HIGH pulse.
static uint8_t _data_pins[4] = {12, 11, 10, 9}; // TODO: Correct?
static uint8_t _button_pins[5] = {0, 1, 2, 3, 4}; // TODO: Correct?
static uint8_t _displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS; // TODO: Looks like everything needed is defined here?
static uint8_t _displaycontrol;
static uint8_t _displaymode;

uint8_t _initialized; // TODO: Remove?

uint8_t _numlines, _currline;

uint8_t _i2cAddr = 0; // TODO: Remove this variable?

static void _digitalWrite(uint8_t p, uint8_t d); // TODO: Change name
static void _pinMode(uint8_t p, uint8_t d); // TODO: Change name?
static void send(uint8_t value, uint8_t mode);
static void write4bits(uint8_t value);

void display_init(void) // TODO: Rename to shield_init?
{
	mcp23017_init();

	display_begin(16, 2, LCD_5x8DOTS);
}

void display_begin(uint8_t cols, uint8_t lines, uint8_t dotsize)
{
	// check if i2c
	// printk("*** DISPLAY BEGIN ***\n");
	mcp23017_begin(_i2cAddr);

	// Controlling the 3 backlight RBG LEDs
	mcp23017_pinMode(8, OUTPUT); // TODO: Give pins name? pin_configure?
	mcp23017_pinMode(6, OUTPUT);
	mcp23017_pinMode(7, OUTPUT);
	// mcp23017_digitalWrite(8, HIGH);
	mcp23017_digitalWrite(6, LOW);
	mcp23017_digitalWrite(7, LOW);

	uint16_t test_out = 0;

	// test_out = mcp23017_readGPIOAB();

	// // test_out &= ~(1 << 8);
	// // test_out &= ~(1 << 7);
	// // test_out &= ~(1 << 6);

	// // test_out |= (1 << 7);
	// // test_out |= (1 << 6);
	// // test_out |= (1 << 8);

	// mcp23017_writeGPIOAB(test_out);

	// display_set_backlight(0x0); // NO NOT WORK PROPERLY FOR PIN B (8->)

	mcp23017_pinMode(_rw_pin, OUTPUT);

	mcp23017_pinMode(_rs_pin, OUTPUT);
	mcp23017_pinMode(_enable_pin, OUTPUT);
	for (uint8_t i = 0; i < 4; i++) {
		mcp23017_pinMode(_data_pins[i], OUTPUT);
	}

	for (uint8_t i = 0; i < 5; i++) {
		mcp23017_pinMode(_button_pins[i], INPUT);
		mcp23017_pullUp(_button_pins[i], HIGH);
	}

	_numlines = lines;
	_currline = 0;

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait
	// 50
	k_sleep(K_USEC(50000));
	// Now we pull both RS and R/W low to begin commands
	// _digitalWrite(_rs_pin, LOW); ***
	_digitalWrite(_enable_pin, LOW);
	printk("*** rw_pin: %d ***\n", _rw_pin);
	_digitalWrite(_rw_pin, LOW);

	// put the LCD into 4 bit or 8 bit mode
	printk("4 BIT!\n");
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46

	// we start in 8bit mode, try to set 4 bit mode
	write4bits(0x03);
	k_sleep(K_USEC(4500)); // wait min 4.1ms

	// second try
	write4bits(0x03);
	k_sleep(K_USEC(4500)); // wait min 4.1ms

	// third go!
	write4bits(0x03);
	k_sleep(K_USEC(150));

	// finally, set to 8-bit interface
	write4bits(0x02);

	// finally, set # lines, font size, etc.
	command(LCD_FUNCTIONSET | _displayfunction);

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display_on();

	// clear it off
	display_clear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	command(LCD_ENTRYMODESET | _displaymode);
}

/********** high level commands, for the user! */
void display_clear()
{
	command(LCD_CLEARDISPLAY); // clear display, set cursor position to zero
	k_sleep(K_USEC(2000)); // this command takes a long time!
}

void display_home()
{
	command(LCD_RETURNHOME); // set cursor position to zero
	k_sleep(K_USEC(2000)); // this command takes a long time!
}

void display_set_cursor(uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (row > _numlines) {
		row = _numlines - 1; // we count rows starting w/0
	}

	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void display_no_display() // TODO: Off
{
	_displaycontrol &= ~LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_on()
{
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void display_no_cursor()
{
	_displaycontrol &= ~LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_cursor()
{
	_displaycontrol |= LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void display_no_blink()
{
	_displaycontrol &= ~LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void display_blink()
{
	_displaycontrol |= LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void display_scroll_display_left(void)
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void display_scroll_display_right(void)
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void display_left_to_right(void)
{
	_displaymode |= LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void display_right_to_left(void)
{
	_displaymode &= ~LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void display_autoscroll(void)
{
	_displaymode |= LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void display_no_autoscroll(void)
{
	_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void display_create_char(uint8_t location, uint8_t charmap[])
{
	location &= 0x7; // we only have 8 locations 0-7
	command(LCD_SETCGRAMADDR | (location << 3));
	for (int i = 0; i < 8; i++) {
		display_write(charmap[i]);
	}
	command(LCD_SETDDRAMADDR); // unfortunately resets the location to 0,0
}

/*********** mid level commands, for sending data/cmds */

void command(uint8_t value)
{
	send(value, LOW);
}

void display_write(uint8_t value)
{
	send(value, HIGH);
}

/************ low level data pushing commands **********/

// little wrapper for i/o writes
static void _digitalWrite(uint8_t p, uint8_t d) // TODO: Remove
{
	mcp23017_digitalWrite(p, d);
}

// Allows to set the backlight, if the LCD backpack is used
void display_set_backlight(uint8_t status)
{
	// check if i2c or SPI
	mcp23017_digitalWrite(8, ~(status >> 2) & 0x1); // ******** THIS DOES NOT WORK PROPERLY ********
	mcp23017_digitalWrite(7, ~(status >> 1) & 0x1);
	mcp23017_digitalWrite(6, ~status & 0x1);
}

// little wrapper for i/o directions
static void _pinMode(uint8_t p, uint8_t d) // TODO: Change name?
{
	mcp23017_pinMode(p, d);
}

// write either command or data, with automatic 4/8-bit selection
static void send(uint8_t value, uint8_t mode)
{
	// printk("send: %d\n", value);

	// _digitalWrite(_rs_pin, mode); // TODO: *** IS THE PROBLEM THAT THIS NEVER GOES HIGH? ***

	if (mode == LOW) {
		mcp23017_pinMode(_rs_pin, OUTPUT);
	} else {
		mcp23017_pinMode(_rs_pin, INPUT);
	}

	// if there is a RW pin indicated, set it low to Write
	if (_rw_pin != 255) {
		_digitalWrite(_rw_pin, LOW);
	}

	write4bits(value >> 4);
	write4bits(value);

	// _digitalWrite(_rs_pin, LOW);
}

static void write4bits(uint8_t value)
{
	uint16_t out = 0;

	out = mcp23017_readGPIOAB();

	printk("OUT BEFORE: %d\n", out);

	// speed up for i2c since its sluggish
	for (int i = 0; i < 4; i++) {
		out &= ~(1 << _data_pins[i]);
		out |= ((value >> i) & 0x1) << _data_pins[i];
	}

	// make sure enable is low
	out &= ~(1 << _enable_pin);

	printk("OUT AFTER: %d\n", out);

	mcp23017_writeGPIOAB(out);

	// pulse enable
	k_sleep(K_USEC(1));
	out |= (1 << _enable_pin);
	mcp23017_writeGPIOAB(out);
	k_sleep(K_USEC(1));
	out &= ~(1 << _enable_pin);
	mcp23017_writeGPIOAB(out);
	k_sleep(K_USEC(100));
}

uint8_t display_read_buttons(void)
{
	uint8_t reply = 0;

	for (uint8_t i = 0; i < 5; i++) {
		reply &= ~((mcp23017_digitalRead(_button_pins[i])) << i);
	}
	return reply;
}
