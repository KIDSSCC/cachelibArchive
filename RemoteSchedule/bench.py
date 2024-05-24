import socket
import subprocess
import threading
import time
import os
import pickle


host = '127.0.0.1'
port = 54000
executable_path = '/home/md/SHMCachelib/Build/tmdb_test'
data_path = '/home/md/workloadData/'

sig_end = 0




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
    #new_partition = [config[1][0]-2, config[1][1], config[1][2]+2]
    #new_config = [config[0], new_partition]
    return new_config
    
'''
construct all partitions
'''
def get_all_partitions():
    all_partitions = []
    for i in range(51):
        all_partitions.append([i])
    return all_partitions


def print_1(line):
    fileName = 'global_record.log'
    print(line)
    return
    with open(fileName, 'a') as logFile:
        print(line, file=logFile)


def execute_tmdb_load(workload):
    complete_workload = data_path + workload + '_load.txt'
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-l']
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process


def execute_tmdb_run(wl):
    file_run = data_path + wl + '_run.txt'
    command_order = [executable_path, '-i', file_run, '-o', wl, '-r']

    itea = 1
    while sig_end == 0:
        process = subprocess.Popen(command_order, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        std_out, _ = process.communicate()
        # print_1('itea is:'+str(itea))
        # print_bytes(wl, std_out)
        # itea = itea+1
        #if itea>2:
        #    break
        
def capture_log(workloads):
    itea = 1
    latency_capture = 'latency_capture.log'

    all_partition = get_all_partitions()
    for i in range(len(all_partition)):
        set_pool_stats([workloads, all_partition[i]])
        #time.sleep(20)
        # write to file
        for wl in workloads:
            hitrate_file_name = wl + "_CacheHitRate.log"
            latency_file_name = wl + "_performance.log"
            message = 'adjust size to:' + str(all_partition[i]) + '\n'
            #print(message)
            with open(hitrate_file_name, 'a') as f:
                f.write(message)
            with open(latency_file_name, 'a') as f:
                f.write(message)
        time.sleep(180)
        
        '''
        hitrates = []
        for wl in workloads:
            hitrate_log_name = wl + '_CacheHitRate.log'
            hitrates.append(get_last_line(hitrate_log_name)[16:])
        print_1(all_partition[i])
        print_1(hitrates)
        '''

        schedule = 'scheduleMangement/findOptimal_' + str(i+1) + '_' + str(len(all_partition)) + '.log'
        file = open(schedule, 'w')
        file.close()
    sig_end = 1


def print_bytes(name, result):
    text = result.decode('utf-8')
    logFile = 'time_record.log'
    print_1('workload is: '+ name)
    for line in text.splitlines():
        print_1('\t'+ line)



if __name__ == '__main__':
    
    workloads = ['uniform20G']
    # start server
    server_path = ['/home/md/SHMCachelib/Build/Server']
    server_proc = subprocess.Popen(server_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    time.sleep(3)


    # load the origin data
    print_1('-----Load Phase-----')
    all_load_proc = []
    for wl in workloads:
        all_load_proc.append(execute_tmdb_load(wl))
        time.sleep(0.2)
    for wl, p in zip(workloads, all_load_proc):
        stdout, stderr = p.communicate()
        print_bytes(wl, stdout)

    print_1('-----Run Phase-----')
    all_run_thread = []
    for wl in workloads:
        all_run_thread.append(threading.Thread(target = execute_tmdb_run, args = (wl,)))
        all_run_thread[-1].start()

    capture_latency = threading.Thread(target = capture_log, args = (workloads,))
    capture_latency.start()
    capture_latency.join()
    print_1('capture finished')
    for t in all_run_thread:
        t.join()
    
    # end server
    server_proc.terminate()

    #new_config = [['zipfian', 'uniform'], [2, 4]]
    #set_pool_stats(new_config)

