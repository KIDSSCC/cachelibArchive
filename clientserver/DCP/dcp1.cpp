#include "dcp.h"

int main(){
    std::vector<std::string> other_nodes = {"127.0.0.1:1413"};
    Node node(1412, other_nodes);
    node.start();

    int stop;
    cin >> stop;
    std::string response = node.sendMessage("Hello from Node 1!");
    std::cout << "Received response: " << response << std::endl;

    cin >> stop;
    node.stop();  // 停止节点
}