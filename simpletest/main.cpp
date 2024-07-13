#include<iostream>
#include <boost/multiprecision/cpp_dec_float.hpp>
using namespace std;
int main(){
	using namespace boost::multiprecision;
	typedef number<cpp_dec_float<100>> high_precision_float;
	high_precision_float number = 1250000;
	high_precision_float number1 = 1249999;

	// 计算以10为底的对数
    high_precision_float result = log10(number);
    high_precision_float result1 = log10(number1);
	// 输出结果
    std::cout.precision(std::numeric_limits<high_precision_float>::digits10);
    std::cout<<result-result1<<std::endl;
}

