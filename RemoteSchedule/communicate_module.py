import socket
import pickle


'''
receive the current config from bench
'''
def receive_config():
    # create the server socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', 1412))
    server_socket.listen(1)
    
    # listening the old_config from the bench
    client_socket, client_address = server_socket.accept()
    received_data = client_socket.recv(1024)
    config = pickle.loads(received_data)
    
    client_socket.close()
    server_socket.close()
    return config


'''
send the new config to bench
'''
def send_config(new_config):
    serialized_config = pickle.dumps(new_config)

    # connect to the bench
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(('127.0.0.1', 1413))
    client_socket.send(serialized_config)
    client_socket.close()
    return


if __name__ == '__main__':
    while True:
        old_config = receive_config()
        print(old_config)
        '''
        eg:
            old_config[0]: ['uniform', 'zipfian']    # name of pool and the turn of pool size and latency
            old_config[1]: [16, 16]                  # current pool size,64MB per unit, for example, 16 means current pool size is 1G
            old_config[2]: [0.0321, 0.0234]          # latency of each workload
            old_config[3]: [0.8254, 0.7563]          # cache hit rate of each workload
        '''
        
        new_config = []
        new_config.append(old_config[0])
        new_partition = [old_config[1][0]-2, old_config[1][1]+2, old_config[1][2]]
        new_config.append(new_partition)

        '''
        eg:
            new_config[0]: ['uniform', 'zipfian']   # be the same as old_config[0]
            new_config[1]: [10, 22]                 # the new cache allocation after the resource tuning
        '''
        send_config(new_config)
