/*-
 * Copyright (c) 2020-2022 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/thread.h>

#include <dev/uart/uart.h>
#include <dev/gpio/gpio.h>
#include <dev/i2c/i2c.h>
#include <dev/i2c/bitbang/i2c_bitbang.h>
#include <dev/bme680/bme680.h>
#include <dev/bme680/bme680_driver.h>
#include <dev/mh_z19b/mh_z19b.h>

static struct bme680_dev gas_sensor;
static struct i2c_bitbang_softc i2c_bitbang_sc;
static struct mdx_device dev_bitbang = { .sc = &i2c_bitbang_sc };

extern struct mdx_device dev_gpiohs;
extern struct mdx_device dev_gpio;
extern struct mdx_device dev_i2c;
extern struct mdx_device dev_uart;

#define	PIN_BME680_SDO	26
#define	PIN_I2C_SCL	28
#define	PIN_I2C_SDA	29
#define	PIN_RELAY	1
#define	PIN_GPIO_LED0	4
#define	PIN_GPIO_LED1	5

static void
i2c_scl(void *arg, bool enable)
{

	if (enable)
		mdx_gpio_configure(&dev_gpiohs, PIN_I2C_SCL,
		    MDX_GPIO_INPUT);
	else
		mdx_gpio_configure(&dev_gpiohs, PIN_I2C_SCL,
		    MDX_GPIO_OUTPUT);
}

static void
i2c_sda(void *arg, bool enable)
{

	if (enable)
		mdx_gpio_configure(&dev_gpiohs, PIN_I2C_SDA,
		    MDX_GPIO_INPUT);
	else
		mdx_gpio_configure(&dev_gpiohs, PIN_I2C_SDA,
		    MDX_GPIO_OUTPUT);
}

static int
i2c_sda_val(void *arg)
{
	uint8_t reg;

	reg = mdx_gpio_get(&dev_gpiohs, PIN_I2C_SDA);

	return (reg & 0x1);
}

static struct i2c_bitbang_ops i2c_ops = {
	.i2c_scl = &i2c_scl,
	.i2c_sda = &i2c_sda,
	.i2c_sda_val = &i2c_sda_val,
};

static int
bme680_sensor_init(void)
{
	int ret;

	i2c_bitbang_init(&dev_bitbang, &i2c_ops);

	mdx_gpio_set(&dev_gpiohs, PIN_I2C_SCL, 0);
	mdx_gpio_set(&dev_gpiohs, PIN_I2C_SDA, 0);

	if (1 == 1) {
		/* For I2C controller */
		ret = bme680_initialize(&dev_i2c, &gas_sensor);
	} else {
		/*
		 * Software bitbang.
		 * TODO: requires K210 FPIOA pins reconfiguration.
		 */
		ret = bme680_initialize(&dev_bitbang, &gas_sensor);
	}

	return (ret);
}

static void
drain_fifo(void)
{
	bool ready;

	for (;;) {
		ready = mdx_uart_rxready(&dev_uart);
		if (!ready)
			break;
		mdx_uart_getc(&dev_uart);
	}
}

static void
boiler_control(int temp)
{

	if (temp < 2400) {
		mdx_gpio_set(&dev_gpiohs, PIN_RELAY, 1);
		mdx_gpio_set(&dev_gpio, PIN_GPIO_LED1, 0);
	} else {
		mdx_gpio_set(&dev_gpiohs, PIN_RELAY, 0);
		mdx_gpio_set(&dev_gpio, PIN_GPIO_LED1, 1);
	}
}

int
main(void)
{
	struct bme680_field_data data;
#if 0
	uint8_t reply[9];
	uint8_t req[9];
	uint32_t co2;
#endif
	int error;

	printf("Hello world!\n");

	/* Relay */
	mdx_gpio_configure(&dev_gpiohs, PIN_RELAY, MDX_GPIO_OUTPUT);
	mdx_gpio_set(&dev_gpiohs, PIN_RELAY, 0);

	/* Turn off LED. */
	mdx_gpio_set(&dev_gpio, PIN_GPIO_LED0, 1);
	mdx_gpio_set(&dev_gpio, PIN_GPIO_LED1, 1);

#if 0
	int val;
	val = 0;
	while (1) {
		if (val == 0)
			val = 1;
		else
			val = 0;
		mdx_gpio_set(&dev_gpiohs, PIN_RELAY, val);
		mdx_gpio_set(&dev_gpio, PIN_GPIO_LED0, val);
		mdx_usleep(5000000);
	}
#endif

	/*
	 * bme680 SDO
	 * 1: sets i2c address to 0x77
	 * 0: sets i2c address to 0x76
	 */
	mdx_gpio_configure(&dev_gpiohs, PIN_BME680_SDO, MDX_GPIO_OUTPUT);
	mdx_gpio_set(&dev_gpiohs, PIN_BME680_SDO, 1);

	/*
	 * Note that for I2C interface bme680 CS pin must be set to HIGH
	 * at same time as power supplied to bme680, otherwise it will
	 * be locked to SPI protocol.
	 */

	error = bme680_sensor_init();
	if (error)
		printf("could not initialize bme680\n");

	drain_fifo();

#if 0
	mh_z19b_set_range_req(req, 2000);
	mh_z19b_cycle(&dev_uart, req, reply, 2);
#endif

	/* Measurement range changed, wait a bit. */
	mdx_usleep(5000000);

	drain_fifo();

#if 0
	mh_z19b_read_co2_req(req);
#endif

	while (1) {
#if 0
		mh_z19b_cycle(&dev_uart, req, reply, 9);

		error = mh_z19b_read_co2_reply(reply, &co2);
		if (error) {
			printf("Failed to read CO2 data, error %d\n",
			    error);
			continue;
		}
#endif

		error = bme680_read_data(&gas_sensor, &data);
		if (error == 0) {
			printf("cpu%d CO2 %d ppm, temp %d\n", PCPU_GET(cpuid),
			    0, data.temperature);
			boiler_control(data.temperature);
		} else
			printf("Failed to read bme680 data, error %d\n",
			    error);

		bme680_trigger(&gas_sensor);
		mdx_usleep(5000000);
	}

	return (0);
}
