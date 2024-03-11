import socket
import subprocess


host = '127.0.0.1'
port = 1412




'''
get init cache stat and pool stat
'''
def get_init_stat():
    global sock
    message = "generate init cache"
    sock.sendall(message.encode())

    # wait the response
    response = sock.recv(1024).decode()
    print("Received: ", response)


def execute_tmdb_load(workload):
    executable_path = '/home/md/SHMCachelib/Build/tmdb_test'
    complete_workload = '/home/md/simpleData/' + workload
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload]
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process





if __name__ == '__main__':
    # link the target program
    # sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # sock.connect((host, port))

    # get_init_stat()
    process_1 = execute_tmdb_load('uniform')
    process_2 = execute_tmdb_load('zipfian')

    # wait the process finished
    stdout_2, stderr_2 = process_2.communicate()
    stdout_1, stderr_1 = process_1.communicate()

    print('proc1 result', stdout_1)
    print('proc2 result', stdout_2)


    # close the socket
    # sock.close()


