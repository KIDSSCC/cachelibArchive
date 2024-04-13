#include<iostream>
#include "YCSBC.h"

using namespace std;
int main(){
	size_t numKeys = 10;
	double skew = 1.0;
	ZipfianDistribution zipfian(numKeys, skew);

	unordered_map<size_t, size_t> workload;
	size_t numRequests = 100;
	for(size_t i = 0; i<numRequests; ++i){
		size_t key = zipfian.nextKey();
		workload[key]++;
	}
	for(const auto&pair:workload){
		cout<<"Key is: "<<pair.first<<" Requests: "<<pair.second<<endl;
	}
	return 0;
}
