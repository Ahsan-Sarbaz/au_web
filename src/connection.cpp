#include "connection.hpp"
#include "request.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/socket.h>

constexpr size_t max_request_size = 4 * 1024 * 1024;

Connection::Connection() : buffer(max_buffer_size)
{
}

bool Connection::handle_read()
{
    auto start = std::chrono::high_resolution_clock::now();

    if (!request_in_progress)
    {
        request_data.clear();
        request_data.reserve(max_buffer_size);
        request_in_progress = true;
    }

    size_t current_request_size = request_data.size();
    int iterations = 0;
    bool request_completed = false;

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
                // No more data available right now
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
        iterations++;

        // Create a temporary request object to check if the request is complete
        Request temp_request = Request::from_content(std::vector<char>(request_data));
        if (temp_request.check_complete())
        {
            request_completed = true;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "handle_read took " << duration.count() << "us, " << iterations << " iterations\n";

    // Only process complete requests
    if (request_completed)
    {
        Request request = Request::from_content(std::move(request_data));
        request.parse();
        request.print();

        // Send response
        std::string response = request.create_response();
        if (send(handle, response.data(), response.size(), 0) == -1)
        {
            std::cerr << "Error sending response: " << strerror(errno) << std::endl;
        }

        request_in_progress = false;
        return true; // Close connection after response (HTTP/1.0 behavior)
    }

    return false; // Keep connection open, waiting for more data
}