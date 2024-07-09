#include <iostream>
#include <folly/ConcurrentSkipList.h>
#include <folly/init/Init.h>

using namespace std;
using SkipListType = folly::ConcurrentSkipList<int>::Accessor;
//using NodeType = folly::ConcurrentSkipList<string>::Node*;

int main(int argc, char* argv[]){
	folly::Init init(&argc, &argv);
	SkipListType list = SkipListType(folly::ConcurrentSkipList<int>::create(10));
	list.insert(20);
	list.insert(30);
	list.insert(40);

	auto node = list.find(20);
	if(node){
		cout<<"Found: "<<*node->getItem()<<endl;
	}else{
		cout<<"20 not found"<<endl;
	}

	auto node = list.find(50);
	if(node){
		cout<<"Found: "<<*node->getItem()<<endl;
	}else{
		cout<<"50 not found"<<endl;
	}
}
