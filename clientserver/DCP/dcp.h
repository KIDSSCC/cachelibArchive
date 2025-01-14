#include<iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

using namespace std;

class Node{
public:

    using getFunctionType = bool (*)(const std::string&, char*);
    Node(int port, const std::vector<std::string>& other_node_addresses, getFunctionType func1)
        : port(port), other_node_addresses(other_node_addresses){
        
        getFunc = func1;
        recv_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (recv_sockfd < 0) {
            perror("Error creating receive socket");
            exit(1);
        }
         // 设置接收端口
        recv_addr.sin_family = AF_INET;
        recv_addr.sin_addr.s_addr = INADDR_ANY;
        recv_addr.sin_port = htons(port);
        // 绑定接收socket
        if (bind(recv_sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
            perror("Error binding receive socket");
            exit(1);
        }

        // 创建发送消息的UDP socket
        send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (send_sockfd < 0) {
            perror("Error creating send socket");
            exit(1);
        }
    }

    ~Node() {
        close(recv_sockfd);  // 关闭接收socket
        close(send_sockfd);  // 关闭发送socket
    }

    void start() {
        // 启动接收消息的线程
        running = true;
        listener_thread = std::thread(&Node::listenForMessages, this);
    }

    void stop() {
        running = false;
        listener_thread.join();
    }

    // 向其他节点发送消息并接收回应
    std::string sendMessage(const std::string& message) {
        std::string response = "";
        for (const auto& address : other_node_addresses) {
            size_t colon_pos = address.find(':');
            string dest_ip = address.substr(0, colon_pos);
            int dest_port = std::stoi(address.substr(colon_pos + 1));
            response = sendMessageToNode(dest_ip, dest_port, message);
            if(response != "")
                break;
        }
        return response;  // 返回最后一个节点的回应
    }
private:
    void listenForMessages() {
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        if (setsockopt(recv_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt failed");
        }

        char buffer[1024];
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);

        while(running){
            ssize_t len = recvfrom(recv_sockfd, buffer, sizeof(buffer), 0, 
                                   (struct sockaddr*)&sender_addr, &addr_len);
            if (len < 0) {
                continue;
            }
            buffer[len] = '\0';
            string receivedMessage(buffer);
            if(buffer[0]=='0'){
                // 收到其他节点发送的查询请求
                string response = "Here is server node at " + to_string(port);
                ssize_t sent_len = sendto(recv_sockfd, response.c_str(), response.length(), 0,
                                    (struct sockaddr*)&sender_addr, addr_len);
                if (sent_len < 0) {
                    perror("Error sending response");
                }
            }
        }
    }

    std::string sendMessageToNode(const std::string& dest_address, const int& dest_port, const std::string& message) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(dest_port);
        dest_addr.sin_addr.s_addr = inet_addr(dest_address.c_str());

        if (sendto(send_sockfd, message.c_str(), message.length(), 0, 
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("Error sending message");
            return "";
        }

        std::cout << "Sent message to " << dest_address << ": " << message << std::endl;

        // 接收回应
        char buffer[1024];
        socklen_t addr_len = sizeof(dest_addr);
        ssize_t len = recvfrom(send_sockfd, buffer, sizeof(buffer), 0, 
                               (struct sockaddr*)&dest_addr, &addr_len);
        if (len < 0) {
            perror("Error receiving response");
            return "";
        }

        buffer[len] = '\0';  // Null-terminate the received string
        return std::string(buffer);
    }

private:
    int recv_sockfd;  // 接收消息的socket
    int send_sockfd;  // 发送消息的socket
    int port;
    struct sockaddr_in recv_addr;
    std::vector<std::string> other_node_addresses;
    std::thread listener_thread;
    bool running;

    getFunctionType getFunc; 
};