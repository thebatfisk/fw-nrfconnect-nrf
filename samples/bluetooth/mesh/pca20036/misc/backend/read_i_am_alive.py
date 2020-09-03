import socket
import ctypes
import binascii
import nodes_info

CMD_I_AM_ALIVE_BYTE = 0x02

rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx_sock.bind(("0.0.0.0", 10001))

node_version_dict = dict()

list_of_nodes = list()
count = 0

while True:
    raw_data, addr = rx_sock.recvfrom(4096)  # buffer size is 4096 bytes

    if (len(raw_data) == 34):
        opcode = int(raw_data[4])

        if opcode == CMD_I_AM_ALIVE_BYTE:
            own_mac = binascii.hexlify(bytearray(raw_data[5:11]))
            ip = binascii.hexlify(bytearray(raw_data[11:15]))
            mac = bytearray(raw_data[15:21])
            version = int(ctypes.c_uint8(raw_data[21]).value)

            mac_hex = mac.hex().upper()
            mac_string = mac_hex[0:2] + ":" + mac_hex[2:4] + ":" + mac_hex[4:6] + ":" + mac_hex[6:8] + ":" + mac_hex[8:10] + ":" + mac_hex[10:12]

            if str(mac) in node_version_dict:
                current_version = node_version_dict[str(mac)]

                if (version > current_version):
                    node_version_dict[str(mac)] = version
                    
                    for i in range(len(nodes_info.nodes_mac_list_21)):
                        if (own_mac == binascii.hexlify(nodes_info.nodes_mac_list_21[i])):
                            list_of_nodes.append(i + 1)
                            list_of_nodes.sort()
                            print(list_of_nodes)
                            break

                    print("MAC: " + mac_string + " Version: " + str(version))
                    
                    count += 1
                    print("Count: " + str(count))
            else:
                node_version_dict[str(mac)] = version

                for i in range(len(nodes_info.nodes_mac_list_21)):
                    if (own_mac == binascii.hexlify(nodes_info.nodes_mac_list_21[i])):
                        list_of_nodes.append(i + 1)
                        list_of_nodes.sort()
                        print(list_of_nodes)
                        break

                print("MAC: " + mac_string + " Version: " + str(version))
                
                count += 1
                print("Count: " + str(count))
