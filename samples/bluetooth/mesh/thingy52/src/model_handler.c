/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

#define LED_INTERVAL 2000

static struct device *chip_temp_sensor;
static struct device *io_expander;

static struct device *spkr_pwr_ctrl;
static struct device *spkr_pwm;
static u32_t pwm_frequency = 880;

const nrfx_pdm_config_t pdm_config = NRFX_PDM_DEFAULT_CONFIG(26, 25);
static int16_t pdm_buffer_a[256];
static int16_t pdm_buffer_b[256];
static const int pdm_sampling_freq = 16000;
static const int fft_size = 256;
static const int spectrum_size = fft_size / 2;
int spectrum[128];

static struct k_delayed_work microphone_work;

static int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	if (NRFX_ERROR_BASE_NUM - nrfx_err) {
		return 1;
	} else {
		return 0;
	}
}

static void microphone_handler(struct k_work *work)
{
	if (fft_analyzer_available()) {
		fft_analyzer_read(spectrum, spectrum_size);
		for (int i = 0; i < spectrum_size; i++) {
			if (spectrum[i]) {
				int frequency =
					((i * pdm_sampling_freq) / fft_size);
				printk("Frequency: %d\tAmplitude: %d\n",
				       frequency, spectrum[i]);
				if (frequency < 1200) {
					gpio_port_set_bits_raw(io_expander,
							       BIT(5) | BIT(7));
					gpio_port_clear_bits_raw(io_expander,
								 BIT(6));
				} else if (frequency < 1500) {
					gpio_port_set_bits_raw(io_expander,
							       BIT(6) | BIT(7));
					gpio_port_clear_bits_raw(io_expander,
								 BIT(5));
				} else if (frequency < 1800) {
					gpio_port_set_bits_raw(io_expander,
							       BIT(5) | BIT(6));
					gpio_port_clear_bits_raw(io_expander,
								 BIT(7));
				}
				break;
			}
		}
	}

	k_delayed_work_submit(&microphone_work, K_MSEC(200));
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	int err;

	if ((pressed & BIT(0))) {
		err = nrfx_err_code_check(nrfx_pdm_start());

		if (err) {
			printk("Error starting PDM\n");
		} else {
			printk("PDM started\n");
		}

		k_delayed_work_submit(&microphone_work, K_NO_WAIT);
	} else if ((!pressed & BIT(0))) {
		gpio_port_set_bits_raw(io_expander, BIT(5) | BIT(6) | BIT(7));

		k_delayed_work_cancel(&microphone_work);

		err = nrfx_err_code_check(nrfx_pdm_start());

		if (err) {
			printk("Error stopping PDM\n");
		} else {
			printk("PDM stopped\n");
		}
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void pdm_event_handler(nrfx_pdm_evt_t const *p_evt)
{
	if (p_evt->error) {
		printk("PDM overflow error\n");
	} else if (p_evt->buffer_requested && p_evt->buffer_released == 0) {
		nrfx_pdm_buffer_set(pdm_buffer_a,
				    sizeof(pdm_buffer_a) /
					    sizeof(pdm_buffer_a[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == pdm_buffer_a) {
		fft_analyzer_update(pdm_buffer_a, 256);
		nrfx_pdm_buffer_set(pdm_buffer_b,
				    sizeof(pdm_buffer_b) /
					    sizeof(pdm_buffer_b[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == pdm_buffer_b) {
		fft_analyzer_update(pdm_buffer_b, 256);
		nrfx_pdm_buffer_set(pdm_buffer_a,
				    sizeof(pdm_buffer_a) /
					    sizeof(pdm_buffer_a[0]));
	}
}

static void button_and_led_init(void)
{
	int err;

	dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if (io_expander == NULL) {
		printk("Could not initiate I/O expander\n");
	}

	err = gpio_pin_configure(io_expander, 5, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure green LED pin\n");
	}

	err = gpio_pin_configure(io_expander, 6, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure blue LED pin\n");
	}

	err = gpio_pin_configure(io_expander, 7, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure red LED pin\n");
	}
}

static void chip_temp_sensor_init(void)
{
	chip_temp_sensor =
		device_get_binding(DT_PROP(DT_NODELABEL(temp), label));

	if (chip_temp_sensor == NULL) {
		printk("Could not initiate temperature sensor\n");
	} else {
		printk("Temperature sensor (%s) initiated\n",
		       chip_temp_sensor->name);
	}
}

static void speaker_init(void)
{
	int err;
	u32_t pwm_period = 1000000U / pwm_frequency;

	spkr_pwr_ctrl = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));

	if (spkr_pwr_ctrl == NULL) {
		printk("Could not initiate speaker power control pin\n");
		return;
	}

	err = gpio_pin_configure(spkr_pwr_ctrl, 29, GPIO_OUTPUT | GPIO_PULL_UP);

	if (err) {
		printk("Could not configure speaker power pin\n");
		return;
	} else {
		printk("Speaker power pin configured\n");
	}

	spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));

	if (spkr_pwm == NULL) {
		printk("Could not initiate speaker PWM\n");
		return;
	} else {
		printk("Speaker PWM (%s) initiated\n", spkr_pwm->name);
	}

	err = pwm_pin_set_usec(spkr_pwm, 27, pwm_period, pwm_period / 2U, 0);

	if (err) {
		printk("Error %d: failed to set PWM values\n", err);
		return;
	}
}

static void microphone_init(void)
{
	int err;

	err = fft_analyzer_configure(256);

	if (err) {
		printk("Error configuring FFT analyzer\n");
	} else {
		printk("FFT analyzer configured\n");
	}

	err = gpio_pin_configure(io_expander, 9, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure speaker power pin\n");
		return;
	} else {
		printk("Speaker power pin configured\n");
	}

	gpio_port_set_bits_raw(io_expander, BIT(9));

	err = nrfx_err_code_check(
		nrfx_pdm_init(&pdm_config, pdm_event_handler));

	if (err) {
		printk("Error initiating PDM\n");
	} else {
		printk("PDM initiated\n");
	}

	err = nrfx_err_code_check(nrfx_pdm_start());

	if (err) {
		printk("Error starting PDM\n");
	} else {
		k_delayed_work_init(&microphone_work, microphone_handler);
		printk("PDM started\n");
	}
}

/** Sensor server */

static int chip_temp_get(struct bt_mesh_sensor *sensor,
			 struct bt_mesh_msg_ctx *ctx, struct sensor_value *rsp)
{
	int err;

	sensor_sample_fetch(chip_temp_sensor);

	err = sensor_channel_get(chip_temp_sensor, SENSOR_CHAN_DIE_TEMP, rsp);

	if (err) {
		printk("Error getting temperature sensor data (%d)\n", err);
	}

	return err;
}

static struct bt_mesh_sensor chip_temp = {
	.type = &bt_mesh_sensor_present_dev_op_temp,
	.get = chip_temp_get,
};

static struct bt_mesh_sensor presence_sensor = {
	.type = &bt_mesh_sensor_presence_detected,
};

static struct bt_mesh_sensor *const sensors[] = {
	&chip_temp,
	&presence_sensor,
};

static struct bt_mesh_sensor_srv sensor_srv =
	BT_MESH_SENSOR_SRV_INIT(sensors, ARRAY_SIZE(sensors));

/** Configuration server definition */
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = IS_ENABLED(CONFIG_BT_MESH_RELAY),
	.beacon = BT_MESH_BEACON_ENABLED,
	.frnd = IS_ENABLED(CONFIG_BT_MESH_FRIEND),
	.gatt_proxy = IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY),
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(2, 20),
};

static void attention_on(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(spkr_pwr_ctrl, 29, 1);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power on\n");
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(spkr_pwr_ctrl, 29, 0);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power off\n");
	}
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub),
					BT_MESH_MODEL_SENSOR_SRV(&sensor_srv)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	button_and_led_init();
	chip_temp_sensor_init();
	speaker_init();
	microphone_init();

	return &comp;
}
