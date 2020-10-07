/***************************************************
  This is a library for the MCP23017 i2c port expander

  These displays use I2C to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <zephyr.h>
#include <drivers/i2c.h>
#include "gw_display_mcp23017.h"

struct device *i2c_dev;

void mcp23017_init(void)
{
	uint8_t reset_a_inputs[2] = {MCP23017_IODIRA, 0xFF};
    uint8_t reset_b_inputs[2] = {MCP23017_IODIRB, 0xFF};

    i2c_dev = device_get_binding(DT_PROP(DT_NODELABEL(i2c0), label));

    i2c_write(i2c_dev, reset_a_inputs, 2, MCP23017_ADDRESS);
    i2c_write(i2c_dev, reset_b_inputs, 2, MCP23017_ADDRESS);
}

void mcp23017_pin_configure(uint8_t p, uint8_t d)
{
	uint8_t iodir;
	uint8_t iodiraddr;
    uint8_t buf[2];

	if (p > 15) {
		return;
	}

	if (p < 8) {
		iodiraddr = MCP23017_IODIRA;
	} else {
		iodiraddr = MCP23017_IODIRB;
		p -= 8;
	}

    i2c_write_read(i2c_dev, MCP23017_ADDRESS, &iodiraddr, 1, &iodir, 1);

	if (d == INPUT) {
		iodir |= 1 << p;
	} else {
		iodir &= ~(1 << p);
	}

    buf[0] = iodiraddr;
    buf[1] = iodir;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS);
}

uint16_t mcp23017_read_gpio_ab()
{
    uint8_t wr_buf = MCP23017_GPIOA;
    uint8_t rd_buf[2];

    i2c_write_read(i2c_dev, MCP23017_ADDRESS, &wr_buf, 1, rd_buf, 2);

	return rd_buf[1] <<= 8 | rd_buf[0];
}

void mcp23017_write_gpio_ab(uint16_t ba)
{
    uint8_t buf[3];

    buf[0] = MCP23017_GPIOA;
    buf[1] = (ba & 0xFF);
    buf[2] = (ba >> 8);
    i2c_write(i2c_dev, buf, 3, MCP23017_ADDRESS);
}

void mcp23017_write_pin(uint8_t p, uint8_t d)
{
	uint8_t gpio;
	uint8_t gpioaddr, olataddr;
    uint8_t buf[2];

	if (p > 15)
		return;

	if (p < 8) {
		olataddr = MCP23017_OLATA;
		gpioaddr = MCP23017_GPIOA;
	} else {
		olataddr = MCP23017_OLATB;
		gpioaddr = MCP23017_GPIOB;
		p -= 8;
	}

	/* Read the current GPIO output latches */
    i2c_write_read(i2c_dev, MCP23017_ADDRESS, &olataddr, 1, &gpio, 1);

	if (d == HIGH) {
		gpio |= 1 << p;
	} else {
		gpio &= ~(1 << p);
	}

    buf[0] = gpioaddr;
    buf[1] = gpio;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS);
}

void mcp23017_pull_up(uint8_t p, uint8_t d)
{
	uint8_t gppu;
	uint8_t gppuaddr;
	uint8_t buf[2];

	if (p > 15)
		return;

	if (p < 8)
		gppuaddr = MCP23017_GPPUA;
	else {
		gppuaddr = MCP23017_GPPUB;
		p -= 8;
	}

    i2c_write_read(i2c_dev, MCP23017_ADDRESS, &gppuaddr, 1, &gppu, 1);

	if (d == HIGH) {
		gppu |= 1 << p;
	} else {
		gppu &= ~(1 << p);
	}

	buf[0] = gppuaddr;
    buf[1] = gppu;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS);
}

uint8_t mcp23017_read_pin(uint8_t p) // TODO: Check that it works
{
	uint8_t gpioaddr;
	uint8_t rd_buf = 0;

	if (p > 15)
		return 0;

	if (p < 8)
		gpioaddr = MCP23017_GPIOA;
	else {
		gpioaddr = MCP23017_GPIOB;
		p -= 8;
	}

	i2c_write_read(i2c_dev, MCP23017_ADDRESS, &gpioaddr, 1, &rd_buf, 1);

	return (rd_buf >> p) & 0x1;
}
