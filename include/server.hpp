#pragma once
#include <array>
#include <unordered_map>

#include "connection.hpp"

constexpr size_t max_connections = 1024;

class Server
{
  public:
    explicit Server(int port);
    void run();

  private:
    int port;
    int server_socket;
    int epoll_fd;

    std::array<Connection, max_connections> connections;
    std::unordered_map<int, size_t> socket_to_connection;
};