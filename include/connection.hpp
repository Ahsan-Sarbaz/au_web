#pragma once

#include "request.hpp"
#include <optional>
#include <vector>

constexpr size_t max_buffer_size = 1024 * 4;

class Connection
{
    friend class Server;

  public:
    Connection();
    std::optional<Request> handle_request();

  private:
    int handle = -1;
    std::vector<char> buffer;
    std::vector<char> request_data;
    bool request_in_progress = false;
};