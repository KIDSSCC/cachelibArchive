import socket
import subprocess

cgroup_path = '/sys/fs/cgroup/blkio/'
host = '127.0.0.1'
port = 54000


'''
delete all old groups
'''
def clear_groups():
    command = 'ls -d ' + cgroup_path + '*/'
    print(command)
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    stdout = result.stdout
    if 'cannot' in stdout or "" == stdout:
        # no groups
        #print('no groups need to clear')
        return
    all_groups = stdout.strip().split('\n')
    all_groups = [line.replace(cgroup_path, "")[:-1] for line in all_groups]
    num = len(all_groups)
    for group in all_groups:
        delete_command = 'cgdelete -r blkio:' + group
        print(delete_command)
        subprocess.run(delete_command, shell=True, text=True, capture_output=False)
    #print('clear {} groups'.format(num))

'''
add a pool in cache, just for test
'''
def addpool(poolname):
    # link the target program
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    message = "A:" + poolname
    sock.sendall(message.encode())

    # wait the response
    response = sock.recv(1024).decode()
    # print(response)

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
set cache size
params:
    workloads:      [wl1, wl2, wl3]
    cache_size:     [5, 6, 7]
'''
def set_cache_size(workloads, cache_size):
    # link the server program
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    curr_config = [workloads, cache_size]
    serialized_data = '\n'.join([' '.join(map(str, row)) for row in curr_config])
    serialized_data = 'S:' + serialized_data
    #print(serialized_data)

    # send to server
    sock.sendall(serialized_data.encode())
    sock.close()

'''
set cpu cores
params:
    procs:      [pid1, pid2, pid3]
    cores:      [3, 4, 5]
'''
def set_cpu_cores(procs, cores):
    core_index = 0
    for i in range(len(procs)):
        cpu_to_set = map(str, range(core_index, core_index + cores[i]))
        allocated_cpu = ','.join(cpu_to_set)
        #print(allocated_cpu)

        command = 'taskset -cp ' + allocated_cpu + ' ' + str(procs[i])
        print(command)

        result = subprocess.run(command, shell=True, text=True, capture_output=True)
        if result.returncode == 0:
            print('CPU affinity set successfully.')

        core_index = core_index + cores[i]

'''
set I/O bandwidth
'''
def set_bandwidth(procs, bandwidths):
    # total is 100
    for i in range(len(procs)):
        group_name = 'group_' + str(procs[i])
        # check the group exist
        check_command = 'cgget -g blkio:' + group_name
        # print(check_command)
        check_res = subprocess.run(check_command, shell=True, text=True, capture_output=True)
        if 'cannot' in check_res.stderr:
            # group non-exist,need to create new group
            #print('{} non-exist'.format(group_name))
            # create new group
            create_command = 'cgcreate -g blkio:' + group_name
            # print(create_command)
            subprocess.run(create_command, shell=True, text=True, capture_output=False)
            # add proc to group
            classify_command = 'cgclassify -g blkio:' + group_name + ' ' + str(procs[i])
            # print(classify_command)
            subprocess.run(classify_command, shell=True, text=True, capture_output=False)
        # adjust the weigh
        adjust_command = 'cgset -r blkio.throttle.read_bps_device="8:16 ' + str(bandwidths[i] * 10485760) + '" ' + group_name
        # print(adjust_command)
        subprocess.run(adjust_command, shell=True, text=True, capture_output=False)


if __name__ == '__main__':
    # delete all old group
    clear_groups()
    
    addpool('tmdb')
    addpool('mysql')
    addpool('leveldb')

    curr_pool_name = []
    curr_pool_size = []
    # test get pool stats
    print('current pool size')
    curr_pool_stats = get_pool_stats()
    for key in curr_pool_stats.keys():
        curr_pool_name.append(key)
        curr_pool_size.append(curr_pool_stats[key])
        print('{} -> {}'.format(key, curr_pool_stats[key]))

    # test set pool size
    curr_pool_size = [curr_pool_size[0]+4, curr_pool_size[1]-2, curr_pool_size[2]-2]
    set_cache_size(curr_pool_name, curr_pool_size)

    curr_pool_stats = get_pool_stats()
    print('after adjust pool size')
    for key in curr_pool_stats.keys():
        curr_pool_name.append(key)
        curr_pool_size.append(curr_pool_stats[key])
        print('{} -> {}'.format(key, curr_pool_stats[key]))
