import serial
from time import sleep

def powerCycleNode(node_index):

    serial_port = serial.Serial(port='COM1', baudrate=9600, timeout=None, parity=serial.PARITY_NONE, bytesize=serial.EIGHTBITS, stopbits=serial.STOPBITS_ONE, xonxoff=False)

    interface_name = "interface gi1/0/" + str(node_index) + "\n"

    print()
    print(interface_name)

    serial_port.flushInput()

    print("Opened port " + serial_port.name)

    for i in range(10):

        serial_port.write("\n".encode())

        bytes_to_read = serial_port.inWaiting()

        # Give the line a small amount of time to receive data ("make inWaiting() not return 0")
        sleep(.5)

        # Receive all the bytes
        while bytes_to_read < serial_port.inWaiting():
            bytes_to_read = serial_port.inWaiting()
            sleep(1)

        # Read the data
        try:
            data = serial_port.read(bytes_to_read).decode()
            print(data)
        except UnicodeDecodeError:
            data = ""

        if (data[-10:] == "User Name:"):
            print("Doing the commands...")
            serial_port.write("bt_mesh\n".encode())
            sleep(1)
            serial_port.write("NordicSemi1234\n".encode())
            sleep(1)
            serial_port.write("configure terminal\n".encode())
            sleep(1)
            serial_port.write(interface_name.encode())
            sleep(1)
            serial_port.write("power inline never\n".encode())

            sleep(3)

            bytes_to_read = serial_port.inWaiting()
            sleep(.5)

            while bytes_to_read < serial_port.inWaiting():
                bytes_to_read = serial_port.inWaiting()
                sleep(1)

            data = serial_port.read(bytes_to_read).decode()
            print(data)

            sleep(60)

            serial_port.write("power inline auto\n".encode())

            sleep(3)

            bytes_to_read = serial_port.inWaiting()
            sleep(.5)

            while bytes_to_read < serial_port.inWaiting():
                bytes_to_read = serial_port.inWaiting()
                sleep(1)

            data = serial_port.read(bytes_to_read).decode()
            print(data)

            sleep(1)
            serial_port.write("exit\n".encode())
            sleep(1)
            serial_port.write("exit\n".encode())
            sleep(1)
            serial_port.write("exit\n".encode())

            sleep(120)

            break

        print("Typing newline one more time...")
        sleep(2)

    serial_port.close()
    print("Port closed")
    print()
