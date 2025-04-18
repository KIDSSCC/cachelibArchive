#include <iostream>
#include <folly/init/Init.h>

using namespace std;


int main(int argc, char* argv[]){
	folly::Init init(&argc, &argv);
	cout<<"hello, world"<<endl;
}
