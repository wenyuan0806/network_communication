import sys
import socket
from ctypes import *

# ------------- Global Variable ------------ #
PORT = 8888
SERVERNAME = '172.22.101.221'
# ------------------------------------------ #

def main():
    server = (SERVERNAME, PORT)
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sockfd.connect(server)
        print('\n Connected to {:s} \n'.format(repr(server)))

        buffer = "Hello, I'm client."
        sockfd.send(buffer.encode())

    except AttributeError as ae:
        print('\n ERROR: Socket creation failed: {} \n'.format(ae))

    except socket.error as se:
        print('\n ERROR: Exception on socket: {} \n'.format(se))

    finally:
        sockfd.close()

if __name__ == "__main__":
    main()