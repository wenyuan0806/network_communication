import socket

# ------------- Global Variable ------------ #
PORT        = 8888
SERVERNAME  = 'localhost'
SERV_TO_CLI = b'\x01'
CLI_TO_SERV = b'\x02'
BUFSIZE     = 256
# ------------------------------------------ #

def createSocket():
    server = (SERVERNAME, PORT)
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.settimeout(1)

    try:
        sockfd.connect(server)
        print('\n Connected to {:s} \n'.format(repr(server)))
    except Exception as e:
        print('\n {} \n'.format(e))

    return sockfd

def main():
    packet = b''

    sockfd = createSocket()

    try:
        sockfd.send(SERV_TO_CLI)
        print("\n Send [{}] to server \n".format(SERV_TO_CLI))

        FILEPATH = '/home/usbaudio/network_communication/tcp/file_transfer/test.txt'
        FILEPATH = '/home/usbaudio/rts3901_sdk_v1.1_turn-key/users/test/UACtest/sin480.wav'
        FILEPATH = '/home/usbaudio/rts3901_sdk_v1.1_turn-key/users/test/UACtest/uactest.c'
        FILEPATH = '/home/usbaudio/rts3901_sdk_v1.1_turn-key/users/test/UACtest/uactest'
        FILEPATH = '/home/usbaudio/rts3901_sdk_v1.1_turn-key/rt_UAC_rw'
        FILEPATH = '/home/usbaudio/rts3901_sdk_v1.1_turn-key/WAVE_48K_16bit_n6dB_60sec.wav'
        
        sockfd.send(FILEPATH.encode())
        print("\n Request [{}] file from server \n".format(FILEPATH))

        file = open('test2', 'wb')

        while True:
            packet = sockfd.recv(BUFSIZE)
            if(len(packet) == 0):
                file.close()
                break
            file.write(packet)

        """ for index in range(-1, -BUFSIZE, -1):
            if data[index] != 0:
                fileData = data[:index+1]
                break """

    except AttributeError as ae:
        print('\n ERROR: Socket creation failed: {} \n'.format(ae))

    except socket.error as se:
        print('\n ERROR: Exception on socket: {} \n'.format(se))

    finally:
        sockfd.close()

if __name__ == "__main__":
    main()