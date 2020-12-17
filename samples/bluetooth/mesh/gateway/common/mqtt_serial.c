#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <net/mqtt.h>
#include <net/buf.h>

#define MQTT_STATIC_SIZE 13

void mqtt_serial_msg_decode(struct net_buf *buf,
			    struct mqtt_publish_param *param)
{
	param->message.topic.qos = net_buf_pull_u8(buf);
	param->message_id = net_buf_pull_le16(buf);
	param->dup_flag = net_buf_pull_u8(buf);
	param->retain_flag = net_buf_pull_u8(buf);
	param->message.topic.topic.size = net_buf_pull_le32(buf);
	param->message.topic.topic.utf8 =
		net_buf_pull_mem(buf, param->message.topic.topic.size);
	param->message.payload.len = net_buf_pull_le32(buf);
	param->message.payload.data =
		net_buf_pull_mem(buf, param->message.payload.len);
}

int mqtt_serial_msg_encode(struct mqtt_publish_param *param,
			   uint8_t *outgoing_buf, uint16_t outgoing_buf_len,
			   uint16_t *msg_len)
{
	if (outgoing_buf_len < (MQTT_STATIC_SIZE + param->message.payload.len +
				param->message.topic.topic.size)) {
		printk("mqtt_msg_encode: Provided buffer is to small\n");
		return -1;
	}

	*msg_len = MQTT_STATIC_SIZE + param->message.payload.len +
		   param->message.topic.topic.size;

	uint8_t buf_idx = 0;

	/* Add QOS and message ID to outgoing packet buffer */
	memcpy(outgoing_buf + buf_idx, &param->message.topic.qos,
	       sizeof(param->message.topic.qos));
	buf_idx += sizeof(param->message.topic.qos);
	memcpy(outgoing_buf + buf_idx, &param->message_id,
	       sizeof(param->message_id));
	buf_idx += sizeof(param->message_id);

	/* Add Retain- and DUP flags to outgoing packet buffer */
	outgoing_buf[buf_idx] = param->dup_flag;
	buf_idx += sizeof(uint8_t);
	outgoing_buf[buf_idx] = param->retain_flag;
	buf_idx += sizeof(uint8_t);

	/* Add Topic and Topic length to outgoing packet buffer */
	memcpy(outgoing_buf + buf_idx, &param->message.topic.topic.size,
	       sizeof(param->message.topic.topic.size));
	buf_idx += sizeof(param->message.topic.topic.size);
	memcpy(outgoing_buf + buf_idx, param->message.topic.topic.utf8,
	       param->message.topic.topic.size);
	buf_idx += param->message.topic.topic.size;

	/* Add Data and Data length to outgoing packet buffer */
	memcpy(outgoing_buf + buf_idx, &param->message.payload.len,
	       sizeof(param->message.payload.len));
	buf_idx += sizeof(param->message.payload.len);
	memcpy(outgoing_buf + buf_idx, param->message.payload.data,
	       param->message.payload.len);

	return 0;
}
