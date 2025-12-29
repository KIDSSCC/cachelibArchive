import re
import matplotlib.pyplot as plt

if __name__ == '__main__':
    all_latency = []
    all_throughput = []
    pattern = r"latency:\s*([\d.]+).*throughput:\s*([\d.]+)"
    with open('schedule.log', 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            match = re.search(pattern, line)

            all_latency.append(float(match.group(1)))
            all_throughput.append(float(match.group(2)))

    
    x = range(len(all_latency))

    fig, ax1 = plt.subplots()
    # 左边 y 轴
    line1, = ax1.plot(x, all_latency, 'b-', label="average latency")
    ax1.set_ylabel("latency(μs)", color='b')
    ax1.tick_params(axis='y', labelcolor='b')

    # 右边 y 轴，共享 x 轴
    ax2 = ax1.twinx()
    line2, = ax2.plot(x, all_throughput, 'r-', label="average throughput")
    ax2.set_ylabel("throughput(records/s)", color='r')
    ax2.tick_params(axis='y', labelcolor='r')

    ax1.set_xlabel("epoch")

    # 图例合并
    lines = [line1, line2]
    labels = [line.get_label() for line in lines]
    ax1.legend(lines, labels, loc="upper left")

    ax1.axvline(x=6, color='gray', linestyle='--', linewidth=1)
    ax1.axvline(x=12, color='gray', linestyle='--', linewidth=1)

    # 添加标题
    plt.title("Cache schedule")

    plt.savefig("output_plot.png", dpi=300, bbox_inches='tight')

    baseline_latency = all_latency[4:7]
    baseline_throughput = all_throughput[4:7]

    schedule_latency = all_latency[34:37]
    schedule_throughput = all_throughput[34:37]

    latency_impr = (sum(baseline_latency) - sum(schedule_latency)) / sum(baseline_latency)
    throughput_impr = (sum(schedule_throughput) - sum(baseline_throughput)) / sum(baseline_throughput)

    print("latency improvment is {:.3%}".format(latency_impr))
    print("throughput_impr is {:.3%}".format(throughput_impr))



