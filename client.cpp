#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

#include "my_TcpSocket.hpp"


int main() {
    my::TcpSocket client;
    client.connectTo("lengtiminf.dpdns.org", 8080);
    client.write("666\n");
    std::cout << client.read() << '\n';
}