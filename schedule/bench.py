import subprocess
import logging
import time
import os
import random
from util import *

# 配置日志输出的基本格式和日志级别
logging.basicConfig(level=logging.INFO)

directory_path  = '/home/md/SHMCachelib/Build'
passwd = 'k15648611412'
disk_bandwidth = 1024 * 1024

def generate_even_list(n, C):
    # 初步分配，每个元素均为 C // n
    base_value = C // n
    result = [base_value] * n
    # 剩余的部分
    remainder = C % n
    
    # 随机分配 remainder
    for i in random.sample(range(n), remainder):
        result[i] += 1
    
    return result

def get_pid(task_name):
    all_pids = []
    for name in task_name:
        proc = subprocess.run(['pidof', name[0]], shell=False, text=True, capture_output=True)
        pid = proc.stdout.strip().split()
        assert len(pid) != 0, '{} not found'.format(name)
        assert len(pid) == 1, '{} have more than one proc'.format(name)
        all_pids.append(pid[0])
    return all_pids

def clear_groups():
    """
    delete existing blkio groups
    Args:

    Returns:
    """
    command = 'ls -d ' + cgroup_path + 'group*/'
    # print(command)
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    stdout = result.stdout
    if 'cannot' in stdout or "" == stdout:
        # no groups
        # print('no groups need to clear')
        return
    all_groups = stdout.strip().split('\n')
    all_groups = [line.replace(cgroup_path, "")[:-1] for line in all_groups]
    
    for group in all_groups:
        delete_command = 'sudo -S cgdelete -r blkio:' + group
        # print(delete_command)
        subprocess.run(delete_command, input=passwd, shell=True, text=True, capture_output=True)

def set_cpu_cores(pids, cores):
    core_index = 28
    if isinstance(cores, list):
        for i in range(len(pids)):
            cpu_to_set = map(str, range(core_index, core_index + cores[i]))
            allocated_cpu = ','.join(cpu_to_set)
            #print(allocated_cpu)

            command = 'taskset -cp ' + allocated_cpu + ' ' + str(pids[i])
            # print(command)

            result = subprocess.run(command, shell=True, text=True, capture_output=True)
            if result.returncode == 0:
                logging.info('CPU affinity set successfully.')

            core_index = core_index + cores[i]
    elif isinstance(cores, int):
        cpu_to_set = map(str, range(core_index, core_index + cores))
        allocated_cpu = ','.join(cpu_to_set)
        for i in range(len(pids)):
            command = 'taskset -cp ' + allocated_cpu + ' ' + str(pids[i])
            print(command)
            result = subprocess.run(command, shell=True, text=True, capture_output=True)
            if result.returncode == 0:
                logging.info('CPU affinity set successfully.')
    else:
        logging.error('Invalid cores type.')
        return
def set_bandwidth(pids, bandwidths):
    """
    Communicating with OS cgroup blkio, adjust bandwidth
    Args:
        procs (list<str>): pid of all workloads
        bandwidths (list<int>): new bandwidth of every workload

    Returns:

    """
    if isinstance(bandwidths, list):
        for i in range(len(pids)):
            group_name = 'group_' + str(pids[i])
            # check the group exist
            check_command = 'sudo -S cgget -g blkio:' + group_name
            # print(check_command)
            check_res = subprocess.run(check_command, input=passwd, shell=True, text=True, capture_output=True)
            if 'cannot' in check_res.stderr:
                # group non-exist,need to create new group
                print('{} non-exist'.format(group_name))
                # create new group
                create_command = 'sudo -S cgcreate -g blkio:' + group_name
                # print(create_command)
                subprocess.run(create_command, input=passwd, shell=True, text=True, capture_output=True)
                # add proc to group
                classify_command = 'sudo -S cgclassify -g blkio:' + group_name + ' ' + str(pids[i])
                # print(classify_command)
                subprocess.run(classify_command, input=passwd, shell=True, text=True, capture_output=True)
            # adjust the weigh
            adjust_command = 'sudo -S cgset -r blkio.throttle.read_bps_device="8:16 ' + \
                            str(bandwidths[i] * disk_bandwidth) + \
                            '" ' + group_name
            # print(adjust_command)
            subprocess.run(adjust_command, input=passwd, shell=True, text=True, capture_output=True)
    elif isinstance(bandwidths, int):
        group_name = 'default_group'
        check_command = 'sudo -S cgget -g blkio:' + group_name
        check_res = subprocess.run(check_command, input=passwd, shell=True, text=True, capture_output=True)
        if 'cannot' in check_res.stderr:
            print('{} non-exist'.format(group_name))
            create_command = 'sudo -S cgcreate -g blkio:' + group_name
            subprocess.run(create_command, input=passwd, shell=True, text=True, capture_output=True)
        for i in range(len(pids)):
            classify_command = 'sudo -S cgclassify -g blkio:' + group_name + ' ' + str(pids[i])
            subprocess.run(classify_command, input=passwd, shell=True, text=True, capture_output=True)
        adjust_command = 'sudo -S cgset -r blkio.throttle.read_bps_device="8:16 ' + \
                        str(bandwidths * disk_bandwidth) + \
                        '" ' + group_name
        subprocess.run(adjust_command, input=passwd, shell=True, text=True, capture_output=True)
    else:
        logging.error('Invalid bandwidth type.')


def close_server():
    host = '127.0.0.1'
    port = 54000
    message = "E:"
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.sendall(message.encode())

def get_binary():
    # # Build目录下获取可执行文件，随机打乱后从中选择5个任务
    bin_files = [[f] for f in os.listdir(directory_path) if f.startswith("bin")]
    mysql_threads = [4, 2, 2, 4, 4]
    i = 0
    for index in range(len(bin_files)):
        bin_files[index].append('--threads')
        if 'mysql' in bin_files[index][0]:
            bin_files[index].append(str(mysql_threads[i]))
            i += 1
        else:
            bin_files[index].append('1')

    random.seed(0)
    random.shuffle(bin_files)

    # 任务数固定为5，cache大小固定为1536
    workload_num = 10
    cache_size = 10240
    target_workloads = bin_files[:workload_num]
    logging.info('----- Target workloads:')
    for item in target_workloads:
        logging.info(item)
    logging.info('----- Target workloads End')
    return target_workloads, cache_size

def operation(args):
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

def pool_resize(names):
    # TODO: cache size schedule
    time.sleep(10)
    print('----- Begin To Schedule')
    allocate = [25, 0, 24, 21, 43]
    set_cache_size(names, allocate)
    time.sleep(5)
    print('----- End To Schedule')
    p_map = get_pool_stats()
    for key in p_map.keys():
        print('---------- {}->{}'.format(key, p_map[key]))



def prepare_phase(target_workloads):
    logging.info('----- Begin To Prepare')
    start_time = time.time()
    prepare_procs = []
    for wl in target_workloads:
        prepare_procs.append(operation([os.path.join(directory_path, wl[0]), '--prepare']))
    for index, p in enumerate(prepare_procs):
        logging.info('{} Prepare Start'.format(target_workloads[index]))
        stdout, _ = p.communicate()
        lines = stdout.decode('utf-8').strip().split('\n')
        if not any('Preparation done' in s for s in lines):
            logging.error('{} Preparation failed'.format(target_workloads[index]))
    # for wl in target_workloads:
    #     proc = operation([os.path.join(directory_path, wl[0]), '--prepare'])
    #     print('{} Prepare Start'.format(wl[0]))
    #     stdout, _ = proc.communicate()
    #     lines = stdout.decode('utf-8').strip().split('\n')
    #     if not any('Preparation done' in s for s in lines):
    #         logging.error('{} Preparation failed'.format(wl[0]))
    end_time = time.time()
    logging.info('----- Prepare Time: {}'.format(end_time - start_time))
    logging.info('----- Prepare Done')

def cache_server(cache_size, pool_size, default_pool, size_conv=None):
    args = ['taskset', '-c', '56-111', './Build/Server']
    if cache_size is not None:
        args.append('-c')
        args.append(str(cache_size))
    if pool_size is not None:
        args.append('-p')
        args.append(str(pool_size))
    if size_conv is not None:
        args.append('-g')
        args.append(str(size_conv))
    if default_pool == 1:
        args.append('-d')
        args.append('1')
    
    out_filename = 'log/server_out.log'
    err_filename = 'log/server_err.log'
    with open(out_filename, 'w') as out_file, open(err_filename, 'w') as err_file:
        process = subprocess.Popen(args, stdout=out_file, stderr=err_file)
    logging.info('----- Start Cache Server')
    time.sleep(5)
    return process

def warmup(target_workloads):
    logging.info('----- Begin To Warmup')
    start_time = time.time()
    procs = []
    for wl in target_workloads:
        tmp = [os.path.join(directory_path, wl[0]), '--cache', '--warmup', '--loginfo', '0', '--profile', 'log/']
        tmp[-1] = tmp[-1] + wl[0]
        procs.append(operation(tmp))
    for index, p in enumerate(procs):
        logging.info('Waiting {}'.format(target_workloads[index][0]))
        stdout, stderr = p.communicate()
    end_time = time.time()
    logging.info('----- Warmup Time: {}'.format(end_time - start_time))
    logging.info('----- Warmup Done')

def run(target_workloads):
    logging.info('----- Begin To Run')
    start_time = time.time()
    procs = []
    for wl in target_workloads:
        tmp = [os.path.join(directory_path, wl[0]), '--cache', '--run', '5', '--maxquery', '10000', wl[1], wl[2]]
        tmp.extend(['--loginfo', '0', '--profile', 'log/'])
        tmp[-1] = tmp[-1] + wl[0]
        procs.append(operation(tmp))
    pids = get_pid(target_workloads)
    set_cpu_cores(pids, generate_even_list(len(target_workloads), 12))
    set_bandwidth(pids, generate_even_list(len(target_workloads), 50))

    for index, p in enumerate(procs):
        logging.info('Waiting {}'.format(target_workloads[index][0]))
        stdout, stderr = p.communicate()
    end_time = time.time()
    logging.info('----- Run Time: {}'.format(end_time - start_time))
    logging.info('----- Run Done')

def warmup_and_run(target_workloads):
    logging.info('----- Begin To Warmup And Run')
    start_time = time.time()
    procs = []
    for wl in target_workloads:
        tmp = [os.path.join(directory_path, wl[0]), '--cache', '--run', '1', '--maxquery', '5000', wl[1], wl[2]]
        tmp.extend(['--loginfo', '0', '--profile', 'log/'])
        tmp[-1] = tmp[-1] + wl[0]
        procs.append(operation(tmp))
    pids = get_pid(target_workloads)
    #TODO:将所有的进程绑定在相同的cpu核心和同一个带宽组里
    set_cpu_cores(pids, 12)
    set_bandwidth(pids, 50)
    for index, p in enumerate(procs):
        logging.info('Waiting {}'.format(target_workloads[index][0]))
        stdout, stderr = p.communicate()
    end_time = time.time()
    logging.info('----- Run Time: {}'.format(end_time - start_time))
    logging.info('----- Run Done')

if __name__ == '__main__':
    clear_groups()
    target_workloads, cache_size = get_binary()
    
    # prepare阶段
    # prepare_phase(target_workloads)
    # 启动cache server, pool_size 256, size_conv = 64
    # server_process = cache_server(cache_size, 1024, 0, 64)
    # run(target_workloads)
    # close_server()
    # server_process.communicate()

    # 针对baseline的测试
    server_process = cache_server(cache_size, 768, 1, 64)
    warmup_and_run(target_workloads)
    close_server()
    server_process.communicate()