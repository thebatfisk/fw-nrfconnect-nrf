import socket

UDP_PORT = 10000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

identifier = bytearray([0xde, 0xad, 0xfa, 0xce])
identifier.reverse()
opcode = bytearray([0x03])
dummy_mac = bytearray([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
is_broadcast = bytearray([0x01])
on = bytearray([0x01])
off = bytearray([0x00])
target_mac = bytearray([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
target_mac.reverse()

package_dfu = identifier + opcode + dummy_mac + is_broadcast + target_mac

sock.sendto(bytearray(package_dfu), ('255.255.255.255', UDP_PORT))
