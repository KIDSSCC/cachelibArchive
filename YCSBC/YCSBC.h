#include<iostream>
#include<random>
#include<vector>
#include<unordered_map>

using namespace std;

class ZipfianDistribution{
private:
	size_t numKeys_;
	double skew_;
	mt19937 gen_;
	uniform_real_distribution<double> dist_;
	vector<double> probabilities_;
public:
	ZipfianDistribution(size_t numKeys, double skew):gen_(random_device()()),dist_(0.0, 1.0){
		numKeys_ = numKeys;
		skew_ = skew;
		calculateProbabilities();
	}
	size_t nextKey(){
		double r = dist_(gen_);
		size_t key = 0;
		double sum = 0.0;
		for(size_t i = 1; i<=numKeys_; ++i){
			sum += probabilities_[i];
			if(sum>=r||i==numKeys_){
				key = i;
				break;
			}
		}
		return key;
	}
	void calculateProbabilities() {
		probabilities_.resize(numKeys_ + 1);
		double sum = 0.0;
		for(size_t i = 1; i<=numKeys_; ++i){
			sum += 1.0/pow(static_cast<double>(i), skew_);
		}
		double c = 1.0 / sum;
		sum = 0.0;
		for(size_t i = 1; i<=numKeys_; ++i){
			sum += c/pow(static_cast<double>(i), skew_);
			probabilities_[i] = sum;
		}
		for(int i = 0;i<probabilities_.size();i++){
			cout<<probabilities_[i]<<endl;
		}
	}
};
