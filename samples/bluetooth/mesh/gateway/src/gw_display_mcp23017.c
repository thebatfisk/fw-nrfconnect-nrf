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

static uint8_t i2caddr;
struct device *i2c_dev;

// TODO: Remove?
// // minihelper
// static inline void wiresend(uint8_t x)
// {
// #if ARDUINO >= 100
// 	WIRE.write((uint8_t)x);
// #else
// 	WIRE.send(x);
// #endif
// }

// static inline uint8_t wirerecv(void)
// {
// #if ARDUINO >= 100
// 	return WIRE.read();
// #else
// 	return WIRE.receive();
// #endif
// }

void mcp23017_init(void)
{
    i2c_dev = device_get_binding(DT_PROP(DT_NODELABEL(i2c0), label));
}

void mcp23017_begin(uint8_t addr)
{
    // uint8_t buf[2]; TODO: Use this?

	if (addr > 7) {
		addr = 7;
	}

	i2caddr = addr;

	// set defaults!
    // TODO: Is creating constant arrays needed?
    static const uint8_t reset_a_inputs[2] = {MCP23017_IODIRA, 0xFF};
    static const uint8_t reset_b_inputs[2] = {MCP23017_IODIRB, 0xFF};
    i2c_write(i2c_dev, reset_a_inputs, 2, MCP23017_ADDRESS | i2caddr);
    i2c_write(i2c_dev, reset_b_inputs, 2, MCP23017_ADDRESS | i2caddr);
}

void mcp23017_pinMode(uint8_t p, uint8_t d) // WORKING?
{
	uint8_t iodir;
	uint8_t iodiraddr;
    uint8_t buf[2];

	// only 16 bits!
	if (p > 15) {
		return;
	}

	if (p < 8) {
		printk("pinMode DIRA\n");
		iodiraddr = MCP23017_IODIRA;
	} else {
		printk("pinMode DIRB\n");
		iodiraddr = MCP23017_IODIRB;
		p -= 8;
	}

	// read the current IODIR
    i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &iodiraddr, 1, &iodir, 1);
	// i2c_write(i2c_dev, &iodiraddr, 1, MCP23017_ADDRESS | i2caddr);
	// i2c_read(i2c_dev, &iodir, 1, MCP23017_ADDRESS | i2caddr);

	if (iodiraddr == MCP23017_IODIRA) {
		printk("DIRA - iodir: %X\n", iodir);
	} else if (iodiraddr == MCP23017_IODIRB) {
		printk("DIRB - iodir: %X\n", iodir);
	}

	// set the pin and direction
	if (d == INPUT) {
		iodir |= 1 << p;
	} else {
		iodir &= ~(1 << p);
	}

	printk("iodir before: %d\n", iodir);

	// write the new IODIR
    buf[0] = iodiraddr;
    buf[1] = iodir;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS | i2caddr);

	// uint8_t test_buf;

	// i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &iodiraddr, 1, &test_buf, 1);
	// i2c_write(i2c_dev, &iodiraddr, 1, MCP23017_ADDRESS | i2caddr);
	// i2c_read(i2c_dev, &test_buf, 1, MCP23017_ADDRESS | i2caddr);

	// printk("iodir after: %d\n", test_buf);
}

uint16_t mcp23017_readGPIOAB()
{
	uint16_t ret = 0; // TODO: Remove?
    uint8_t wr_buf = MCP23017_GPIOA;
    uint8_t rd_buf[2];

	// read the current GPIO output latches
    i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &wr_buf, 1, rd_buf, 2);

    ret = rd_buf[1] <<= 8;
    ret |= rd_buf[0];

	printk("rd_buf: %d - %d\n", rd_buf[0], rd_buf[1]);
	printk("ret: %d\n", ret);

	return ret;
}

void mcp23017_writeGPIOAB(uint16_t ba)
{
    uint8_t buf[3];

    buf[0] = MCP23017_GPIOA;
    buf[1] = (ba & 0xFF);
    buf[2] = (ba >> 8);
    i2c_write(i2c_dev, buf, 3, MCP23017_ADDRESS | i2caddr);
}

void mcp23017_digitalWrite(uint8_t p, uint8_t d)
{
	uint8_t gpio;
	uint8_t gpioaddr, olataddr;
    uint8_t buf[2];

	// only 16 bits!
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

	// read the current GPIO output latches
    i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &olataddr, 1, &gpio, 1);

	// i2c_write(i2c_dev, &olataddr, 1, MCP23017_ADDRESS | i2caddr);
	// k_sleep(K_USEC(100));
	// i2c_read(i2c_dev, &gpio, 1, MCP23017_ADDRESS | i2caddr);

	printk("OLAT: %X - GPIO: %X\n", olataddr, gpioaddr);

	// printk("GPIO BEFORE: %d\n", gpio);

	// set the pin and direction
	printk("********** d: %d - p: %d\n", d, p);
	if (d == HIGH) {
		gpio |= 1 << p;
	} else {
		printk("*** TEST ***\n");
		gpio &= ~(1 << p);
	}

	// write the new GPIO
    buf[0] = gpioaddr;
    buf[1] = gpio;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS | i2caddr);

	uint8_t test_buf;
	i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &gpioaddr, 1, &test_buf, 1);
	// printk("GPIO AFTER: %d\n", test_buf);

}

void mcp23017_pullUp(uint8_t p, uint8_t d) // WORKING
{
	uint8_t gppu;
	uint8_t gppuaddr;
	uint8_t buf[2];

	// only 16 bits!
	if (p > 15)
		return;

	if (p < 8)
		gppuaddr = MCP23017_GPPUA;
	else {
		gppuaddr = MCP23017_GPPUB;
		p -= 8;
	}

	// read the current pullup resistor set
    i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &gppuaddr, 1, &gppu, 1);

	// set the pin and direction
	if (d == HIGH) {
		gppu |= 1 << p;
	} else {
		gppu &= ~(1 << p);
	}

	// write the new GPIO
	buf[0] = gppuaddr;
    buf[1] = gppu;
    i2c_write(i2c_dev, buf, 2, MCP23017_ADDRESS | i2caddr);
}

uint8_t mcp23017_digitalRead(uint8_t p) // WORKING
{
	int err = 0;
	uint8_t gpioaddr;
	uint8_t rd_buf = 0;

	// // only 16 bits!
	if (p > 15)
		return 0;

	if (p < 8)
		gpioaddr = MCP23017_GPIOA;
	else {
		gpioaddr = MCP23017_GPIOB;
		p -= 8;
	}

	printk("gpioaddr: %X\n", gpioaddr);

	// read the current GPIO
	err = i2c_write_read(i2c_dev, MCP23017_ADDRESS | i2caddr, &gpioaddr, 1, &rd_buf, 1);

	// err |= i2c_write(i2c_dev, &gpioaddr, 1, MCP23017_ADDRESS | i2caddr);
	// k_sleep(K_USEC(100));
	// err |= i2c_read(i2c_dev, &rd_buf, 1, MCP23017_ADDRESS | i2caddr);

	printk("DIGITAL RD (%d): %X - addr: %X\n", err, rd_buf, MCP23017_ADDRESS | i2caddr);

	return (rd_buf >> p) & 0x1;
}