#pragma once
#include <array>
#include <unordered_map>

#include "connection.hpp"
#include "router.hpp"

constexpr size_t max_connections = 1024;

class Server
{
  public:
    explicit Server(int port, Router& router);
    void run();

  private:
    int port;
    int server_socket;
    int epoll_fd;

    Router& router;

    std::array<Connection, max_connections> connections;
    std::unordered_map<int, size_t> socket_to_connection;
};