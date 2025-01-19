#include "dcp.h"

bool reback_func(const std::string& param1, char* param2)
{
    strcpy(param2, "Hello, this is node1");
    return true;
}

int main(){
    std::vector<std::string> other_nodes = {"124.70.22.234:1412"};
    Node node(1412, other_nodes, reback_func);
    node.start();

    int stop;
    cin >> stop;
    std::string response = node.sendMessage("0:Hello from Node 1!");
    std::cout << "Received response: " << response << std::endl;

    cin >> stop;
    node.stop();  // 停止节点
}