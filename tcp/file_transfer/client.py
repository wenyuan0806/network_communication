import os
import socket
import binascii
import time

# ------------- Global Variable ------------ #
PORT        = 8888
SERVERNAME  = '172.22.101.221'
SERV_TO_CLI = b'\x01'
CLI_TO_SERV = b'\x02'
BUFSIZE     = 256
sockfd      = 0
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


def getFileFromServer(SERV_FILEPATH, CLI_FILEPATH):
    global sockfd
    # sockfd = createSocket()

    try:
        sockfd.send(SERV_TO_CLI)
        print("\n Send [{}] to server \n".format(SERV_TO_CLI))
        
        SERV_FILEPATH += '\0'
        sockfd.send(SERV_FILEPATH.encode())
        print("\n Request [{}] file from server \n".format(SERV_FILEPATH))

        file = open(CLI_FILEPATH, 'wb')

        fileSize_byte = sockfd.recv(12)
        fileSize = int(fileSize_byte.replace(b'\x00', b'').decode())
        print('\n File size is totally {} bytes \n'.format(fileSize))

        while True:
            data_byte = sockfd.recv(BUFSIZE)
            fileSize -= BUFSIZE
            if(fileSize < 0):
                file.write(data_byte[0:(fileSize+BUFSIZE)])
                break
            file.write(data_byte)

    except Exception as e:
        if 'timed out' in str(e):
            pass
        else:
            print("\n {} \n".format(e))

    time.sleep(0.05)
    # sockfd.close()
    file.close()


def sendFileToServer(SERV_FILEPATH, CLI_FILEPATH):
    global sockfd
    # sockfd = createSocket()

    try:
        sockfd.send(CLI_TO_SERV)
        print("\n Send [{}] to server \n".format(CLI_TO_SERV))
        
        SERV_FILEPATH += '\0'
        sockfd.send(SERV_FILEPATH.encode())
        print("\n Send [{}] file to server \n".format(SERV_FILEPATH))
        time.sleep(0.05)

        file = open(CLI_FILEPATH, 'rb')
        fileSize = str(os.path.getsize(CLI_FILEPATH)) + '\0'
        # print(fileSize)

        # set send buffer size
        sockfd.send(fileSize.encode())
        time.sleep(0.05)

        while True:
            packet = file.read(BUFSIZE)

            sockfd.send(packet)


            # print(binascii.hexlify(packet))

            if(len(packet) == 0):
                break

    except Exception as e:
        if 'timed out' in str(e):
            pass
        else:
            print("\n {} \n".format(e))
    
    time.sleep(0.05)
    # sockfd.close()
    file.close()


def main():
    global sockfd

    sockfd = createSocket()
    
    SERV_FILEPATH   = '/home/usbaudio/network_communication/tcp/file_transfer/server_folder/WAVE_48K_16bit_n6dB_60sec.wav'
    CLI_FILEPATH    = '/home/usbaudio/network_communication/tcp/file_transfer/client_folder/WAVE_48K_16bit_n6dB_60sec.wav'

    getFileFromServer(SERV_FILEPATH, CLI_FILEPATH)

    SERV_FILEPATH   = '/home/usbaudio/network_communication/tcp/file_transfer/server_folder/uactest.c'
    CLI_FILEPATH    = '/home/usbaudio/network_communication/tcp/file_transfer/client_folder/uactest.c'

    getFileFromServer(SERV_FILEPATH, CLI_FILEPATH)

    SERV_FILEPATH   = '/home/usbaudio/network_communication/tcp/file_transfer/server_folder/hp_2_normal_linein_profile.txt'
    CLI_FILEPATH    = '/home/usbaudio/network_communication/tcp/file_transfer/client_folder/hp_2_normal_linein_profile.txt'

    sendFileToServer(SERV_FILEPATH, CLI_FILEPATH)

    SERV_FILEPATH   = '/home/usbaudio/network_communication/tcp/file_transfer/server_folder/uactest'
    CLI_FILEPATH    = '/home/usbaudio/network_communication/tcp/file_transfer/client_folder/uactest'

    sendFileToServer(SERV_FILEPATH, CLI_FILEPATH)


    sockfd.close()
   

if __name__ == "__main__":
    main()