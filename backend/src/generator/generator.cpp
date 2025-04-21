#include "generator.h"

using namespace std;

/*
Generator：生成器类构造函数
params：
    distibution：枚举类，query分布类型
    workingset_size：整型，生成query的最大编号
    params：额外参数。zipfian，exponential类型使用[0],hotspot类型使用[0:1]
*/
Generator::Generator(Distribution distibution, int workingset_size, vector<double> params){
    d_type = distibution;
    max_records = workingset_size;

    //for hotspot
    std::vector<double> intervals = {0};
    std::vector<double> weights;
    
    // 各生成器与计数器设置
    switch (d_type)
    {
        case D_ZIPFIAN:
            zipfian_distrib = zipfian_int_distribution<int>(0, max_records - 1, params[0]);
            break;
        case D_UNIFORM:
            uniform_distrib = std::uniform_int_distribution<int>(0, max_records - 1);
            break;
        case D_HOTSPOT:
            intervals.push_back(params[0] * max_records);
            intervals.push_back(double(max_records));
            weights.push_back(params[1]);
            weights.push_back(1 - params[1]);

            hotspot_distrib = std::piecewise_constant_distribution<double>(intervals.begin(), intervals.end(), weights.begin());
            break;
        case D_EXPONENTIAL:
            exponential_distrib = std::exponential_distribution<double>(params[0]);
            break;
        case D_SEQUENTIAL:
            sequential_counter = 0;
        default:
            break;
    }
}

/*
get_num：根据随机数引擎生成要查询的key的编号
params：
    engine：随机数引擎
return：
    size_t：待访问的key编号
 */
size_t Generator::get_num(std::default_random_engine &engine){
    size_t key = 0;
    switch (d_type)
    {
        case D_ZIPFIAN:
            key = zipfian_distrib(engine) % max_records;
            break;
        case D_UNIFORM:
            key = uniform_distrib(engine) % max_records;
            break;
        case D_HOTSPOT:
            key = int(hotspot_distrib(engine)) % max_records;
            break;
        case D_EXPONENTIAL:
            key =  int(exponential_distrib(engine)) % max_records;
            break;
        case D_SEQUENTIAL:
            key = sequential_counter++;
            if(sequential_counter >= (int)max_records)
                sequential_counter = 0; 
        default:
            break;
    }
    return key;
}

/*
print：打印生成器对象状态信息
 */
void Generator::print(){
    string distribution_name = "";
    switch (d_type)
    {
        case D_ZIPFIAN:
            distribution_name = "Zipfian";
            break;
        case D_UNIFORM:
            distribution_name = "Uniform";
            break;
        case D_HOTSPOT:
            distribution_name = "Hotspot";
            break;
        case D_EXPONENTIAL:
            distribution_name = "Exponential";
            break;
        case D_SEQUENTIAL:
            distribution_name = "Sequential";
        default:
            break;
    }
    cout << "----- Generator Info -----\n";
    cout << "Distribution: " << distribution_name << endl;
    cout << "Max records: " << max_records << endl;
    cout << "----- Generator End -----\n";
}
