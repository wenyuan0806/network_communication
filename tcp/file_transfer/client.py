import socket
import time
import binascii

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

def getFileFromServer(SERV_FILEPATH, CLI_FILEPATH):
    sockfd = createSocket()

    try:
        sockfd.send(SERV_TO_CLI)
        print("\n Send [{}] to server \n".format(SERV_TO_CLI))
        
        sockfd.send(SERV_FILEPATH.encode())
        print("\n Request [{}] file from server \n".format(SERV_FILEPATH))

        file = open(CLI_FILEPATH, 'wb')

        while True:
            packet = sockfd.recv(BUFSIZE)
            if(len(packet) == 0):
                break
            file.write(packet)

    except Exception as e:
        if 'timed out' in str(e):
            pass
        else:
            print("\n {} \n".format(e))

    sockfd.close()
    file.close()

def sendFileToServer(SERV_FILEPATH, CLI_FILEPATH):
    sockfd = createSocket()

    try:
        sockfd.send(CLI_TO_SERV)
        print("\n Send [{}] to server \n".format(CLI_TO_SERV))

        sockfd.send(SERV_FILEPATH.encode())
        print("\n Send [{}] file from server \n".format(SERV_FILEPATH))

        file = open(CLI_FILEPATH, 'rb')

        while True:
            packet = file.read(BUFSIZE)
            print(binascii.hexlify(packet))
            if(len(packet) == 0):
                break

    except Exception as e:
        if 'timed out' in str(e):
            pass
        else:
            print("\n {} \n".format(e))
    
    sockfd.close()
    file.close()

def main():
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
   

if __name__ == "__main__":
    main()