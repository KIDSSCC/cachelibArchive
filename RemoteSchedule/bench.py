import socket
import subprocess
import threading
import time
import os
import pickle


host = '127.0.0.1'
port = 54000




'''
get pool stat
'''
def get_pool_stats():
    # link the target program
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    message = "G:"
    sock.sendall(message.encode())

    # wait the response
    response = sock.recv(1024).decode()
    sock.close()
    deserialized_map = {}
    pairs = response.split(';')[:-1]
    for pair in pairs:
        key,value = pair.split(':')
        deserialized_map[key] = int(value)
    return deserialized_map


'''
set pool stat
'''
def set_pool_stats(new_config):
    # link the target program
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    serialized_data = '\n'.join([' '.join(map(str, row)) for row in new_config])
    serialized_data = 'S:' + serialized_data
    sock.sendall(serialized_data.encode())
    sock.close()

    
'''
get the last line from a file
'''
def get_last_line(fileName):
    with open(fileName, 'rb') as file:
        file.seek(-2,2)
        while file.read(1)!=b'\n':
            file.seek(-2, 1)
        return file.readline().decode().strip()


'''
send the current config to the scheduler and wait the new config
'''
def resource_schedule(config):
    
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(('127.0.0.1', 1412))
    serialized_config = pickle.dumps(config)
    client_socket.send(serialized_config)
    client_socket.close()
    

    # wait the response
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', 1413))
    server_socket.listen(1)

    response_socket, response_address = server_socket.accept()
    serialized_new_config = response_socket.recv(1024)
    new_config = pickle.loads(serialized_new_config)
    server_socket.close()
    response_socket.close()
    
    #new_partition = [config[1][0]-2, config[1][1]+2]
    #example = [config[0], new_partition]
    return new_config
    


def print_1(line):
    fileName = 'global_record.log'
    #print(line)
    #return
    with open(fileName, 'w') as logFile:
        print(line, file=logFile)


def execute_tmdb_load(workload):
    executable_path = '/home/md/SHMCachelib/Build/tmdb_test'
    complete_workload = '/home/md/simpleData/' + workload + '_load.txt'
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-l']
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process


def execute_tmdb_run(wl):
    executable_path = '/home/md/SHMCachelib/Build/tmdb_test'
    file_run = '/home/md/simpleData/' + wl + '_run.txt'
    command_order = [executable_path, '-i', file_run, '-o', wl, '-r']

    itea = 1
    while(True):
        print_1('itea is:'+str(itea))
        process = subprocess.Popen(command_order, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        std_out, _ = process.communicate()
        print_bytes(wl, std_out)
        itea = itea+1
        if itea>200:
            break
        
def capture_log():
    itea = 1
    latency_capture = 'latency_capture.log'
    while itea < 11:
        time.sleep(2)
        pool_size_map = get_pool_stats()
        config = [[],[],[]]
        print(pool_size_map)
        for wl in pool_size_map.keys():
            #lockFileName = wl + '.lock'
            #while os.path.exists(lockFileName):
                #pass
            logFileName = wl + '_performance.log'
            last_line = get_last_line(logFileName)
            #print(wl, last_line)
            config[0].append(wl)
            config[1].append(pool_size_map[wl])
            config[2].append(last_line[11:])
        new_config = resource_schedule(config)
        set_pool_stats(new_config)

        itea = itea + 1


def print_bytes(name, result):
    text = result.decode('utf-8')
    logFile = 'time_record.log'
    print_1('workload is: '+ name)
    for line in text.splitlines():
        print_1('\t'+ line)



if __name__ == '__main__':
    
    # load the origin data
    print_1('-----Load Phase-----')
    process_1 = execute_tmdb_load('uniform')
    time.sleep(1)
    process_2 = execute_tmdb_load('zipfian')

    # wait the process finished
    stdout_2, stderr_2 = process_2.communicate()
    stdout_1, stderr_1 = process_1.communicate()

    print_bytes('uniform', stdout_1)
    print_bytes('zipfian', stdout_2)

    print_1('-----Run Phase-----')
    execute_uniform = threading.Thread(target = execute_tmdb_run, args = ('uniform',))
    execute_uniform.start()
    execute_zipfian = threading.Thread(target = execute_tmdb_run, args = ('zipfian',))
    execute_zipfian.start()

    capture_latency = threading.Thread(target = capture_log)
    capture_latency.start()

    capture_latency.join()
    execute_uniform.join()
    execute_zipfian.join()

    #new_config = [['zipfian', 'uniform'], [2, 4]]
    #set_pool_stats(new_config)

