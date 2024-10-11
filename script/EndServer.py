import socket

host = '127.0.0.1'
port = 54000

if __name__ == '__main__':
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    message = "E:"
    sock.sendall(message.encode())
