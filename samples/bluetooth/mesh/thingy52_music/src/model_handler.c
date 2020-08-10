/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_sx1509b.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include <thingy52_orientation_handler.h>
#include "model_handler.h"

#define PDM_SAMPLING_FREQ 16000
#define FFT_SIZE 512
#define SPECTRUM_SIZE (FFT_SIZE / 2)
#define SPEAKER_PWM_FREQ 880

#define NUM_FREQ_BANDS 3

// TODO: Remove all 'int' that are not err

static struct bt_mesh_whistle_cli whistle_cli[NUM_FREQ_BANDS] = {
	[0 ... NUM_FREQ_BANDS - 1] = BT_MESH_WHISTLE_CLI_INIT
};

struct devices {
	struct device *io_expander;
	struct device *gpio_0;
	struct device *spkr_pwm;
};
static struct devices dev;

struct mic_config {
	nrfx_pdm_config_t pdm_config;
	int16_t pdm_buffer_a[FFT_SIZE];
	int16_t pdm_buffer_b[FFT_SIZE];
	struct k_delayed_work microphone_work;
};
static struct mic_config mic_cfg = {
	.pdm_config = NRFX_PDM_DEFAULT_CONFIG(26, 25),
};

static inline int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	return NRFX_ERROR_BASE_NUM - nrfx_err ? true : false;
}

static int bind_devices(void)
{
	dev.spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));
	dev.gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));
	dev.io_expander =
		device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if ((dev.gpio_0 == NULL) || (dev.spkr_pwm == NULL) ||
	    (dev.io_expander == NULL)) {
		return -1;
	}

	return 0;
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	if (pressed & BIT(0)) {
		int err = 0;
		static bool onoff = true;
		printk("Microphone: %d\n", onoff);

		if (onoff) {
			err |= sx1509b_set_pin_value(dev.io_expander, MIC_PWR,
						     1);
			err |= nrfx_err_code_check(nrfx_pdm_start());
			k_delayed_work_submit(&mic_cfg.microphone_work,
					      K_NO_WAIT);
		} else {
			k_delayed_work_cancel(&mic_cfg.microphone_work);
			err |= nrfx_err_code_check(nrfx_pdm_stop());
			err |= sx1509b_set_pin_value(dev.io_expander, MIC_PWR,
						     0);
		}

		onoff = !onoff;

		if (err) {
			printk("Error running PDM\n");
		}
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void button_and_led_init(void)
{
	int err = 0;

	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pin_configure(dev.io_expander, i, SX1509B_PWM);
	}

	if (err) {
		printk("Initializing buttons and leds failed.\n");
	}
}

static void speaker_init(u32_t pwm_frequency)
{
	int err = 0;
	u32_t pwm_period = 1000000U / pwm_frequency;

	err |= gpio_pin_configure(dev.gpio_0, SPKR_PWR,
				  GPIO_OUTPUT | GPIO_PULL_UP);
	err |= pwm_pin_set_usec(dev.spkr_pwm, SPKR_PWM, pwm_period,
				pwm_period / 2U, 0);

	if (err) {
		printk("Initializing speaker failed.\n");
	}
}

// TODO: Move to thread? Move fft_analyzer? Profile ARM FFT?
// Looks like fft_analyzer_update() is called twice because of
// nrfx driver "calls the event handler twice"
// - Add a flag for seeing witch "buffer that is active"?
static void pdm_event_handler(nrfx_pdm_evt_t const *p_evt)
{
	if (p_evt->error) {
		printk("PDM overflow error\n");
	} else if (p_evt->buffer_requested && p_evt->buffer_released == 0) {
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_a,
				    sizeof(mic_cfg.pdm_buffer_a) /
					    sizeof(mic_cfg.pdm_buffer_a[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_cfg.pdm_buffer_a) {
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_b,
				    sizeof(mic_cfg.pdm_buffer_b) /
					    sizeof(mic_cfg.pdm_buffer_b[0]));
		fft_analyzer_update(mic_cfg.pdm_buffer_a, FFT_SIZE);
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_cfg.pdm_buffer_b) {
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_a,
				    sizeof(mic_cfg.pdm_buffer_a) /
					    sizeof(mic_cfg.pdm_buffer_a[0]));
		fft_analyzer_update(mic_cfg.pdm_buffer_b, FFT_SIZE);
	}
}

static void send_color_helper(int iterator, int value)
{
	static struct bt_mesh_whistle_rgb_msg msg = {
		.ttl = 0,
		.delay = 0xFFFF,
		.speaker_on = false,
	};

	/** Should match NUM_FREQ_BANDS */

	if (iterator == 0) {
		msg.color.red = 0;
		msg.color.green = 0;
		msg.color.blue = value;
		bt_mesh_whistle_cli_rgb_set(&whistle_cli[iterator], NULL, &msg);
	} else if (iterator == 1) {
		msg.color.red = 0;
		msg.color.green = value;
		msg.color.blue = 0;
		bt_mesh_whistle_cli_rgb_set(&whistle_cli[iterator], NULL, &msg);
	} else if (iterator == 2) {
		msg.color.red = value;
		msg.color.green = 0;
		msg.color.blue = 0;
		bt_mesh_whistle_cli_rgb_set(&whistle_cli[iterator], NULL, &msg);
	} else {
		printk("Unknown iterator\n");
	}
}

static void microphone_work_handler(struct k_work *work)
{
	static int spectrum[SPECTRUM_SIZE];
	static int freq_sum[NUM_FREQ_BANDS];
	static int prev_freq_sum[NUM_FREQ_BANDS];
	static int total_sum;
	static int checker;
	static int divider;
	static int adj_freq_sum;

	if (fft_analyzer_available()) {
		int i;

		for (i = 0; i < NUM_FREQ_BANDS; i++) {
			freq_sum[i] = 0;
		}

		fft_analyzer_read(spectrum, SPECTRUM_SIZE);

		/** Should create the amount of for-loops that macthes NUM_FREQ_BANDS */

		for (i = 0; i <= 80; i++) {
			// TODO: Be able to tune this?
			if (spectrum[i] > 200) {
				freq_sum[0] += spectrum[i];
			}
		}

		for (i = 81; i <= 130; i++) {
			// TODO: Be able to tune this?
			if (spectrum[i] > 100) {
				freq_sum[1] += spectrum[i];
			}
		}

		for (i = 131; i <= 255; i++) {
			// TODO: Be able to tune this?
			if (spectrum[i] > 200) {
				freq_sum[2] += spectrum[i];
			}
		}

		total_sum = freq_sum[0] + freq_sum[1] + freq_sum[2];

		if (total_sum) {
			checker = total_sum / 4;
			divider = 6 + total_sum / 1000;
			// printk("divider: %d\n", divider);
		}

		for (i = 0; i < NUM_FREQ_BANDS; i++) {
			if (freq_sum[i]) {
				// printk("%d: %d\n", i, freq_sum[i]);
				if (freq_sum[i] - prev_freq_sum[i] > checker) {
					adj_freq_sum = freq_sum[i] / divider;
					// printk("total_sum: %d\n", total_sum);
					// printk("%d: %d\n", i, adj_freq_sum);

					if (adj_freq_sum > 255) {
						adj_freq_sum = 255;
					}

					send_color_helper(i, adj_freq_sum);

					prev_freq_sum[i] = freq_sum[i];
					// printk("%d - Sum: %d\n", i, freq_sum[i]);
				}
			} else {
				prev_freq_sum[i] = 0;
			}
		}
	}

	/** Should change FFT buffer size to mach interval */
	k_delayed_work_submit(&mic_cfg.microphone_work, K_MSEC(60));
}

static void microphone_init(void)
{
	int err = 0;

	err |= fft_analyzer_configure(FFT_SIZE);
	err |= sx1509b_pin_configure(dev.io_expander, MIC_PWR, SX1509B_OUTPUT);
	mic_cfg.pdm_config.gain_l = 80; // 80 (decimal) is max
	err |= nrfx_err_code_check(
		nrfx_pdm_init(&mic_cfg.pdm_config, pdm_event_handler));

	if (err) {
		printk("Initializing microphone failed.\n");
	}

	k_delayed_work_init(&mic_cfg.microphone_work, microphone_work_handler);
}

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
	int err = 0;

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_led_set_pwm_value(dev.io_expander, i, 255);
	}

	if (err) {
		printk("Error turning on attention light");
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	int err = 0;

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_led_set_pwm_value(dev.io_expander, i, 0);
	}

	if (err) {
		printk("Error turning off attention light");
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
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[0]))),
	BT_MESH_ELEM(
		3, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[1]))),
	BT_MESH_ELEM(
		4, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[2]))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	if (bind_devices()) {
		printk("Failure occured while binding devices\n");
		return &comp;
	}

	button_and_led_init();
	speaker_init(SPEAKER_PWM_FREQ);
	microphone_init();

	return &comp;
}
