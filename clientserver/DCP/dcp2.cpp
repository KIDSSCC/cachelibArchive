#include "dcp.h"

int main(){
    std::vector<std::string> other_nodes = {"127.0.0.1:1412"};
    Node node(1413, other_nodes);
    node.start();

    int stop;
    cin >> stop;
    std::string response = node.sendMessage("Hello from Node 2!");
    std::cout << "Received response: " << response << std::endl;

    cin >> stop;
    node.stop();  // 停止节点
}