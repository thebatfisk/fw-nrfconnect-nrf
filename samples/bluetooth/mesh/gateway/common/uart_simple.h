#include <net/buf.h>

#ifndef BT_MESH_UART_SIMPLE_H__
#define BT_MESH_UART_SIMPLE_H__

typedef void (*rx_cb)(struct net_buf *get_buf);

struct uart_channel_ctx {
	/* Channel ID */
	const uint8_t channel_id;
	/* Channel RC Callback */
	const rx_cb rx_cb;
};

void uart_simple_channel_create(struct uart_channel_ctx *channel_ctx);

void uart_simple_send(struct uart_channel_ctx *channel_ctx, uint8_t *data,
		      uint16_t len);

void uart_simple_init(void (*default_cb)(struct net_buf *get_buf));

#endif /* BT_MESH_UART_SIMPLE_H__ */
