#pragma once

#include <vector>

constexpr size_t max_buffer_size = 1024;

class Connection
{
    friend class Server;

  public:
    Connection();
    bool handle_read();

  private:
    int handle = -1;
    std::vector<char> buffer;
    std::vector<char> request_data;
    bool request_in_progress = false;
};