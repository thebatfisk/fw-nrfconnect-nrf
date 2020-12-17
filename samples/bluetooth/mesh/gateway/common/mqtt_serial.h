#include <net/mqtt.h>
#include <net/buf.h>

#ifndef BT_MESH_MQTT_SERIAL_MSG_H__
#define BT_MESH_MQTT_SERIAL_MSG_H__

void mqtt_serial_msg_decode(struct net_buf *buf,
			    struct mqtt_publish_param *param);

int mqtt_serial_msg_encode(struct mqtt_publish_param *param,
			   uint8_t *outgoing_buf, uint16_t outgoing_buf_len,
			   uint16_t *msg_len);

#endif /* BT_MESH_MQTT_SERIAL_MSG_H__ */
