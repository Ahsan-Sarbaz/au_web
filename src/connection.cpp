#include "connection.hpp"
#include "request.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sys/socket.h>

constexpr size_t max_request_size = 4 * 1024 * 1024;

Connection::Connection() : buffer(max_buffer_size)
{
}

bool Connection::handle_read()
{
    if (!request_in_progress)
    {
        request_data.clear();
        request_data.reserve(max_buffer_size);
        request_in_progress = true;
    }

    size_t current_request_size = request_data.size();

    while (true)
    {
        // Get a chunk of the request
        int bytes_read = recv(handle, buffer.data(), buffer.size(), 0);

        if (bytes_read <= 0)
        {
            if (bytes_read == 0)
            {
                // Connection closed by client
                return true;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else
            {
                // Error
                std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                return true;
            }
        }

        current_request_size += bytes_read;
        // Check if the request is too large
        if (current_request_size > max_request_size)
        {
            std::cerr << "Request too large" << std::endl;
            return true;
        }

        // Append data to request buffer
        request_data.insert(request_data.end(), buffer.data(), buffer.data() + bytes_read);
    }

    static const char pattern[] = "\r\n\r\n";
    auto it = std::search(request_data.begin(), request_data.end(), pattern, pattern + 4);    
    bool is_complete_request = false;
    if ((it != request_data.end()))
    {
        is_complete_request = true;
    }

    // Only process complete requests
    if (is_complete_request)
    {
        auto header_end = it + 4;
        size_t header_size = header_end - request_data.begin();
        Request request = Request::from_content(std::move(request_data), header_size);
        request.parse();
        // request.print();

        // Send response
        std::string response = request.create_response();
        if (send(handle, response.data(), response.size(), 0) == -1)
        {
            std::cerr << "Error sending response: " << strerror(errno) << std::endl;
        }

        request_in_progress = false;
        return true; // Close connection after response (HTTP/1.0 behavior)
    } else {
        std::cout << "Request not completed" << std::endl;
    }

    return false; // Keep connection open, waiting for more data
}