import socket
import time

UDP_PORT = 10000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

identifier = bytearray([0xde, 0xad, 0xfa, 0xce])
identifier.reverse()
opcode = bytearray([0x04])
dummy_mac = bytearray([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
is_broadcast = bytearray([0x01])
on = bytearray([0x01])
off = bytearray([0x00])
target_mac = bytearray([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
target_mac.reverse()

package_on = identifier + opcode + dummy_mac + is_broadcast + on + target_mac
package_off = identifier + opcode + dummy_mac + is_broadcast + off + target_mac

while True:
    print("Sending ON")
    sock.sendto(bytearray(package_on), ('255.255.255.255', UDP_PORT))
    time.sleep(1)

    print("Sending OFF")
    sock.sendto(bytearray(package_off), ('255.255.255.255', UDP_PORT))
    time.sleep(1)
