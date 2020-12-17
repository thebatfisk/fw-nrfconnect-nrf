#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <random/rand32.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <modem/lte_lc.h>
#if defined(CONFIG_MODEM_KEY_MGMT)
#include <modem/modem_key_mgmt.h>
#endif
#if defined(CONFIG_LWM2M_CARRIER)
#include <lwm2m_carrier.h>
#endif
#include "uart_simple.h"
#include "mqtt_serial.h"
#include "prov_conf_common.h"
#include "gw_nfc.h"
#include "gw_display_shield.h"

#include "certificates.h"

/* Forward declarations */
void mqtt_serial_rx_callback(struct net_buf *get_buf);
void prov_conf_rx_callback(struct net_buf *get_buf);

/* Provisioning and configuration serial channel */
static struct uart_channel_ctx prov_conf_serial_chan = {
	.channel_id = 2,
	.rx_cb = prov_conf_rx_callback,
};
/* MQTT serial channel */
static struct uart_channel_ctx mqtt_serial_chan = {
	.channel_id = 1,
	.rx_cb = mqtt_serial_rx_callback,
};
/* MQTT RX thread stack */
static K_THREAD_STACK_DEFINE(mqtt_rx_thread_stack, 1536);
/* MQTT RX thread context */
static struct k_thread mqtt_rx_thread_ctx;
/* MQTT RX buffer */
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
/* MQTT TX buffer */
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
/* MQTT client context */
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];
/* MQTT client context */
static struct mqtt_client client;
/* MQTT broker context */
static struct sockaddr_storage broker;
/* MQTT file descriptor */
static struct pollfd fds;
/* MQTT broker user */
struct mqtt_utf8 user = {
	.utf8 = CONFIG_MQTT_USER,
	.size = sizeof(CONFIG_MQTT_USER) - 1,
};
/* MQTT broker password */
struct mqtt_utf8 password = {
	.utf8 = CONFIG_MQTT_PW,
	.size = sizeof(CONFIG_MQTT_PW) - 1,
};

static int certificates_provision(void)
{
	int err = modem_key_mgmt_write(CONFIG_MQTT_TLS_SEC_TAG,
				       MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				       CA_CERTIFICATE, strlen(CA_CERTIFICATE));

	if (err) {
		printk("Failed to provision certificate, Err: %d\n", err);
	} else {
		printk("Sucessfully Provisioned the certificate\n");
	}

	return err;
}

static int mqtt_serial_publish(const struct mqtt_publish_param *param,
			       uint8_t *data, size_t len)
{
	struct mqtt_publish_param out_param;

	out_param.message.topic.qos = param->message.topic.qos;
	out_param.message_id = param->message_id;
	out_param.dup_flag = param->dup_flag;
	out_param.retain_flag = param->retain_flag;
	out_param.message.topic.topic.size = param->message.topic.topic.size;
	out_param.message.topic.topic.utf8 = param->message.topic.topic.utf8;
	out_param.message.payload.len = len;
	out_param.message.payload.data = data;

	uint8_t out_buf[256];
	uint16_t msg_len;
	int err = mqtt_serial_msg_encode(&out_param, out_buf, sizeof(out_buf),
					 &msg_len);

	if (!err) {
		uart_simple_send(&mqtt_serial_chan, out_buf, msg_len);
	}

	return 0;
}

static int publish_get_payload(struct mqtt_client *c, size_t length)
{
	uint8_t *buf = payload_buf;
	uint8_t *end = buf + length;

	if (length > sizeof(payload_buf)) {
		return -EMSGSIZE;
	}

	while (buf < end) {
		int ret = mqtt_read_publish_payload(c, buf, end - buf);

		if (ret < 0) {
			int err;

			if (ret != -EAGAIN) {
				return ret;
			}

			printk("Failed to read MQTT payload, Err: %d\n", ret);

			err = poll(&fds, 1,
				   CONFIG_MQTT_KEEPALIVE * MSEC_PER_SEC);
			if (err > 0 && (fds.revents & POLLIN) == POLLIN) {
				continue;
			} else {
				return -EIO;
			}
		}

		if (ret == 0) {
			return -EIO;
		}

		buf += ret;
	}

	return 0;
}

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		if (evt->result == 0) {
			printk("MQTT client connected!\n");

			struct mqtt_topic subscribe_topic = {
				.topic = { .utf8 = CONFIG_MQTT_SUB_TOPIC,
					   .size = strlen(
						   CONFIG_MQTT_SUB_TOPIC) },
				.qos = MQTT_QOS_1_AT_LEAST_ONCE
			};
			const struct mqtt_subscription_list subscription_list = {
				.list = &subscribe_topic,
				.list_count = 1,
				.message_id = 1234
			};

			mqtt_subscribe(&client, &subscription_list);
		}

		break;
	}

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		err = publish_get_payload(c, p->message.payload.len);
		if (err >= 0) {
			mqtt_serial_publish(p, payload_buf,
					    p->message.payload.len);
		} else {
			printk("Failed to retrive on-air MQTT paiload, Err: %d\n",
			       err);

			err = mqtt_disconnect(c);
			if (err) {
				printk("Could not disconnect, Err %d\n", err);
			} else {
				printk("Disconnected from the MQTT client\n");
			}
		}
		break;
	}

	case MQTT_EVT_DISCONNECT:
	case MQTT_EVT_PUBACK:
	case MQTT_EVT_SUBACK:
	case MQTT_EVT_PINGRESP:
	default:
		if (evt->result != 0) {
			printk("A failure occored during MQTT event handling\n");
		}
		break;
	}
}

static int broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = { .ai_family = AF_INET,
				  .ai_socktype = SOCK_STREAM };

	err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
	if (err) {
		printk("Failed to get the address info, Err: %d\n", err);
		return -ECHILD;
	}

	addr = result;

	/* Look for address of the broker. */
	while (addr != NULL) {
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);
			char ipv4_addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
					->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr,
				  sizeof(ipv4_addr));
			printk("IPv4 Address found %s\n", ipv4_addr);
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u\n",
			       (unsigned int)addr->ai_addrlen,
			       (unsigned int)sizeof(struct sockaddr_in),
			       (unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
	}

	freeaddrinfo(result);

	return err;
}

static int client_init(struct mqtt_client *client)
{
	int err;

	mqtt_client_init(client);

	err = broker_init();
	if (err) {
		printk("Failed to initialize broker connection\n");
		return err;
	}

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
	client->client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
	client->password = &password;
	client->user_name = &user;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* MQTT transport configuration */
	struct mqtt_sec_config *tls_cfg = &(client->transport).tls.config;
	static sec_tag_t sec_tag_list[] = { CONFIG_MQTT_TLS_SEC_TAG };

	client->transport.type = MQTT_TRANSPORT_SECURE;
	tls_cfg->peer_verify = CONFIG_MQTT_TLS_PEER_VERIFY;
	tls_cfg->cipher_count = 0;
	tls_cfg->cipher_list = NULL;
	tls_cfg->sec_tag_count = ARRAY_SIZE(sec_tag_list);
	tls_cfg->sec_tag_list = sec_tag_list;
	tls_cfg->hostname = CONFIG_MQTT_BROKER_HOSTNAME;
	tls_cfg->session_cache = IS_ENABLED(CONFIG_MQTT_TLS_SESSION_CACHING) ?
					 TLS_SESSION_CACHE_ENABLED :
					 TLS_SESSION_CACHE_DISABLED;

	return err;
}

static void fds_init(struct mqtt_client *c)
{
	fds.fd = c->transport.tls.sock;
	fds.events = POLLIN;
}

static void modem_configure(void)
{
	int err;

	err = lte_lc_init_and_connect();
	__ASSERT(err == 0, "LTE link could not be established.");
	printk("LTE Link Connected!\n");
}

static void mqtt_rx_thread(void *p1, void *p2, void *p3)
{
	int err = 0;
	while (1) {
		err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
		if (err < 0) {
			printk("ERROR: poll %d\n", errno);
			break;
		}

		err = mqtt_live(&client);
		if ((err != 0) && (err != -EAGAIN)) {
			printk("ERROR: mqtt_live %d\n", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				printk("ERROR: mqtt_input %d\n", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			printk("POLLERR\n");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			printk("POLLNVAL\n");
			break;
		}
		k_yield();
	}

	printk("Disconnecting MQTT client...\n");

	err = mqtt_disconnect(&client);
	if (err) {
		printk("Could not disconnect MQTT client. Error: %d\n", err);
	}
}

static void mqtt_rx_thread_create(void)
{
	k_thread_create(&mqtt_rx_thread_ctx, mqtt_rx_thread_stack,
			K_THREAD_STACK_SIZEOF(mqtt_rx_thread_stack),
			mqtt_rx_thread, NULL, NULL, NULL, K_PRIO_COOP(6), 0,
			K_NO_WAIT);
	k_thread_name_set(&mqtt_rx_thread_ctx, "MQTT RX");
}

void mqtt_serial_rx_callback(struct net_buf *get_buf)
{
	struct mqtt_publish_param param;

	mqtt_serial_msg_decode(get_buf, &param);
	mqtt_publish(&client, &param);
}

K_SEM_DEFINE(sem_gw_system_state, 0, 1);
K_SEM_DEFINE(sem_choose_room, 0, 1);
K_SEM_DEFINE(sem_prov_link_active, 0, 1);
K_SEM_DEFINE(sem_get_unprov_dev_num, 0, 1);
K_SEM_DEFINE(sem_device_added, 0, 1);
K_SEM_DEFINE(sem_get_model_info, 0, 1);

enum gw_system_states {
	s_start = 0,
	s_idle,
	s_blink_dev,
	s_prov_conf_dev,
	s_nfc_start,
	s_nfc_stop,
};

const struct room_info rooms[] = {
	[0] = { .name = "Living room   ", .group_addr = BASE_GROUP_ADDR },
	[1] = { .name = "Kitchen       ", .group_addr = BASE_GROUP_ADDR + 1 },
	[2] = { .name = "Bedroom       ", .group_addr = BASE_GROUP_ADDR + 2 },
	[3] = { .name = "Bathroom      ", .group_addr = BASE_GROUP_ADDR + 3 },
	[4] = { .name = "Hallway       ", .group_addr = BASE_GROUP_ADDR + 4 }
};

static int gw_system_state = s_start;
static bool take_sem = true;

static uint8_t curr_but_pressed;
static uint8_t nfc_uuid[16];
static bool uuid_from_nfc;
static bool update_unprov_devs;
static uint8_t chosen_dev;
static uint8_t room_iterator;
static bool room_chosen;
static bool prov_link_active;
static uint8_t unprov_dev_num;
static struct model_info mod_inf;

static struct k_delayed_work button_work;

static void button_work_handler(struct k_work *work)
{
	uint8_t button_state;
	uint8_t serial_opcode;

	button_state = display_read_buttons();

	if ((button_state & BUTTON_UP) && (curr_but_pressed != BUTTON_UP)) {
		printk("BUTTON UP\n");
		curr_but_pressed = BUTTON_UP;
	} else if (button_state & BUTTON_DOWN &&
		   (curr_but_pressed != BUTTON_DOWN)) {
		printk("BUTTON DOWN\n");
		curr_but_pressed = BUTTON_DOWN;
	} else if (button_state & BUTTON_LEFT &&
		   (curr_but_pressed != BUTTON_LEFT)) {
		printk("BUTTON LEFT\n");
		curr_but_pressed = BUTTON_LEFT;

		if (gw_system_state == s_start || gw_system_state == s_idle) {
			gw_system_state = s_nfc_start;
			k_sem_give(&sem_gw_system_state);
		} else if (gw_system_state == s_blink_dev) {
			serial_opcode = oc_prov_link_active;
			uart_simple_send(&prov_conf_serial_chan, &serial_opcode,
					 1);
			k_sem_take(&sem_prov_link_active, K_SECONDS(3));

			if (!prov_link_active) {
				if (chosen_dev > 0) {
					chosen_dev--;
					k_sem_give(&sem_gw_system_state);
				}
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			if (room_iterator > 0) {
				room_iterator--;
				k_sem_give(&sem_choose_room);
			}
		}
	} else if (button_state & BUTTON_RIGHT &&
		   (curr_but_pressed != BUTTON_RIGHT)) {
		printk("BUTTON RIGHT\n");
		curr_but_pressed = BUTTON_RIGHT;

		if (gw_system_state == s_nfc_start) {
			gw_system_state = s_nfc_stop;
			k_sem_give(&sem_gw_system_state);
		} else if (gw_system_state == s_blink_dev) {
			serial_opcode = oc_prov_link_active;
			uart_simple_send(&prov_conf_serial_chan, &serial_opcode,
					 1);
			k_sem_take(&sem_prov_link_active, K_SECONDS(3));

			if (!prov_link_active) {
				serial_opcode = oc_unprov_dev_num;
				uart_simple_send(&prov_conf_serial_chan,
						 &serial_opcode, 1);
				k_sem_take(&sem_get_unprov_dev_num,
					   K_SECONDS(3));

				if (chosen_dev < unprov_dev_num - 1) {
					chosen_dev++;
					k_sem_give(&sem_gw_system_state);
				}
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			if (room_iterator <
			    (sizeof(rooms) / sizeof(rooms[0]) - 1)) {
				room_iterator++;
				k_sem_give(&sem_choose_room);
			}
		}
	} else if (button_state & BUTTON_SELECT &&
		   (curr_but_pressed != BUTTON_SELECT)) {
		printk("BUTTON SELECT\n");
		curr_but_pressed = BUTTON_SELECT;

		if (gw_system_state == s_start) {
			gw_system_state = s_idle;
			k_sem_give(&sem_gw_system_state);
		} else if (gw_system_state == s_idle) {
			serial_opcode = oc_unprov_dev_num;
			uart_simple_send(&prov_conf_serial_chan, &serial_opcode,
					 1);
			k_sem_take(&sem_get_unprov_dev_num, K_SECONDS(3));

			if (unprov_dev_num > 0) {
				gw_system_state = s_blink_dev;
				k_sem_give(&sem_gw_system_state);
			}
		} else if (gw_system_state == s_blink_dev) {
			serial_opcode = oc_prov_link_active;
			uart_simple_send(&prov_conf_serial_chan, &serial_opcode,
					 1);
			k_sem_take(&sem_prov_link_active, K_SECONDS(3));

			if (!prov_link_active) {
				gw_system_state = s_prov_conf_dev;
				k_sem_give(&sem_gw_system_state);
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			room_chosen = true;
			k_sem_give(&sem_choose_room);
		}
	} else if (!button_state) {
		curr_but_pressed = 0;
	}

	k_delayed_work_submit(&button_work, K_MSEC(100));
}

static struct k_delayed_work unprov_devs_work;

static void unprov_devs_work_handler(struct k_work *work)
{
	uint8_t serial_opcode;

	serial_opcode = oc_unprov_dev_num;
	uart_simple_send(&prov_conf_serial_chan, &serial_opcode, 1);
	k_sem_take(&sem_get_unprov_dev_num, K_SECONDS(3));

	if (update_unprov_devs) {
		display_set_cursor(0, 0);
		display_write_number(unprov_dev_num);
	}

	k_delayed_work_submit(&unprov_devs_work, K_MSEC(500));
}

void nfc_rx(struct gw_nfc_rx_data data)
{
	if (data.length == 16 && gw_system_state == s_nfc_start) {
		memcpy(nfc_uuid, data.value, 16);
		uuid_from_nfc = true;

		gw_system_state = s_prov_conf_dev;
		k_sem_give(&sem_gw_system_state);
	}
}

struct gw_nfc_cb nfc_cb = { .rx = nfc_rx };

void prov_conf_rx_callback(struct net_buf *get_buf)
{
	uint8_t opcode = net_buf_pull_u8(get_buf);

	if (opcode == oc_prov_link_active) {
		prov_link_active = (bool)net_buf_pull_u8(get_buf);
		k_sem_give(&sem_prov_link_active);
	} else if (opcode == oc_unprov_dev_num) {
		unprov_dev_num = net_buf_pull_u8(get_buf);
		k_sem_give(&sem_get_unprov_dev_num);
	} else if (opcode == oc_device_added) {
		k_sem_give(&sem_device_added);
	} else if (opcode == oc_get_model_info) {
		mod_inf.srv_count = net_buf_pull_le16(get_buf);
		mod_inf.cli_count = net_buf_pull_le16(get_buf);
		k_sem_give(&sem_get_model_info);
	}
}

void main(void)
{
	int err = 0;
	uint8_t serial_message[20];

	printk("MQTT bridge nrf9160 started\n");

	display_shield_init();
	display_set_cursor(0, 0);
	display_write_string("Connecting to");
	display_set_cursor(0, 1);
	display_write_string("server...");

	certificates_provision();
	modem_configure();

	err |= client_init(&client);
	err |= mqtt_connect(&client);
	fds_init(&client);

	if (err != 0) {
		printk("MQTT initialization failed\n");
		return;
	}

	uart_simple_init(NULL);
	uart_simple_channel_create(&mqtt_serial_chan);
	uart_simple_channel_create(&prov_conf_serial_chan);

	mqtt_rx_thread_create();

	gw_nfc_register_cb(&nfc_cb);
	gw_nfc_init();

	k_delayed_work_init(&button_work, button_work_handler);
	k_delayed_work_submit(&button_work, K_NO_WAIT);

	k_delayed_work_init(&unprov_devs_work, unprov_devs_work_handler);

	display_set_cursor(0, 0);
	display_write_string("BT Mesh Gateway");
	display_set_cursor(0, 1);
	display_write_string("Press SELECT");

	while (1) {
		if (take_sem) {
			k_sem_take(&sem_gw_system_state, K_FOREVER);
		}

		take_sem = true;

		if (gw_system_state == s_idle) {
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(4, 0);
			display_write_string("unprov devs");
			display_set_cursor(0, 1);
			display_write_string("<- NFC or SELECT");
			update_unprov_devs = true;
			k_delayed_work_submit(&unprov_devs_work, K_NO_WAIT);

		} else if (gw_system_state == s_blink_dev) {
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(4, 0);
			display_write_string("unprov devs");
			display_set_cursor(0, 1);
			display_write_string("Blinking dev ");
			display_write_number(chosen_dev);

			serial_message[0] = oc_blink_device;
			serial_message[1] = chosen_dev;
			uart_simple_send(&prov_conf_serial_chan, serial_message,
					 2);

			update_unprov_devs = true;
			k_delayed_work_submit(&unprov_devs_work, K_NO_WAIT);

		} else if (gw_system_state == s_prov_conf_dev) {
			k_delayed_work_cancel(&unprov_devs_work);
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(0, 0);
			display_write_string("Provisioning...");

			if (uuid_from_nfc) {
				serial_message[0] = oc_provision_device;
				serial_message[1] = 0xff;
				memcpy(&serial_message[2], nfc_uuid, 16);
				uart_simple_send(&prov_conf_serial_chan,
						 serial_message, 18);

				gw_nfc_stop();
				uuid_from_nfc = false;
			} else {
				serial_message[0] = oc_provision_device;
				serial_message[1] = chosen_dev;
				uart_simple_send(&prov_conf_serial_chan,
						 serial_message, 2);
			}

			k_sem_take(&sem_device_added, K_SECONDS(10));

			serial_message[0] = oc_get_model_info;
			uart_simple_send(&prov_conf_serial_chan, serial_message,
					 1);
			k_sem_take(&sem_get_model_info, K_SECONDS(10));

			printk("Server count: %d - Client count: %d\n",
			       mod_inf.srv_count, mod_inf.cli_count);

			room_iterator = 0;

			for (int i = 0; i < mod_inf.srv_count; i++) {
				room_chosen = false;
				display_clear();
				display_set_cursor(0, 0);
				display_write_string("Light ");
				display_write_number(i);
				while (!room_chosen) {
					display_set_cursor(0, 1);
					display_write_string(
						rooms[room_iterator].name);
					k_sem_take(&sem_choose_room, K_FOREVER);
				}

				serial_message[0] = oc_configure_server;
				serial_message[1] = i;
				serial_message[2] =
					rooms[room_iterator].group_addr >> 8;
				serial_message[3] =
					rooms[room_iterator].group_addr &
					0x00ff;
				uart_simple_send(&prov_conf_serial_chan,
						 serial_message, 4);
			}

			for (int i = 0; i < mod_inf.cli_count; i++) {
				room_chosen = false;
				display_clear();
				display_set_cursor(0, 0);
				display_write_string("Light Switch ");
				display_write_number(i);
				while (!room_chosen) {
					display_set_cursor(0, 1);
					display_write_string(
						rooms[room_iterator].name);
					k_sem_take(&sem_choose_room, K_FOREVER);
				}

				serial_message[0] = oc_configure_client;
				serial_message[1] = i;
				serial_message[2] =
					rooms[room_iterator].group_addr >> 8;
				serial_message[3] =
					rooms[room_iterator].group_addr &
					0x00ff;
				uart_simple_send(&prov_conf_serial_chan,
						 serial_message, 4);
			}

			gw_system_state = s_idle;
			take_sem = false;
		} else if (gw_system_state == s_nfc_start) {
			k_delayed_work_cancel(&unprov_devs_work);
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(0, 0);
			display_write_string("Scanning NFC...");
			display_set_cursor(0, 1);
			display_write_string("-> for NFC off");

			gw_nfc_start();
		} else if (gw_system_state == s_nfc_stop) {
			gw_nfc_stop();

			gw_system_state = s_idle;
			take_sem = false;
		}
	}
}
