#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <chrono>
#include <fcntl.h>
#include <netinet/in.h>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <algorithm>

constexpr size_t max_connections = 1024;
constexpr size_t max_buffer_size = 1024;
constexpr size_t max_request_size = 4 * 1024 * 1024;
int epoll_fd;

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Error getting socket flags: " << strerror(errno) << std::endl;
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Error setting socket to non-blocking: " << strerror(errno) << std::endl;
    }
}

struct Header
{
    std::string_view name;
    std::string_view value;
};

enum class Method
{
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
    UNKNOWN
};

struct Request
{
    std::vector<char> content;
    std::vector<Header> headers;
    std::vector<std::string_view> lines;
    std::string_view path;
    Method method = Method::UNKNOWN;
    bool is_complete = false;

    static Request from_content(std::vector<char>&& content)
    {
        Request request;
        request.content = std::move(content);
        return request;
    }

    bool check_complete() {
        static const char pattern[] = "\r\n\r\n";
        auto it = std::search(content.begin(), content.end(), pattern, pattern + 4);
        return it != content.end();
    }
        
    // this takes around (~3us)
    void parse()
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        if (content.empty()) {
            std::cerr << "Empty request content" << std::endl;
            return;
        }
        
        // Ensure content is null-terminated for safety
        if (content.back() != '\0') {
            content.push_back('\0');
        }

        char* data = content.data();
        char* end = data + content.size() - 1; // Exclude the null terminator
        
        // Parse HTTP headers
        char* line_start = data;
        char* ptr = data;
        
        auto a = std::chrono::high_resolution_clock::now();

        // Parse request lines (takes around ~1us)
        while (ptr < end - 1) {
            if (ptr[0] == '\r' && ptr[1] == '\n') {
                size_t line_length = ptr - line_start;
                
                if (line_length == 0) {
                    // Empty line marks the end of headers
                    ptr += 2; // Skip CRLF
                    break;
                }
                
                lines.emplace_back(line_start, line_length);
                ptr += 2; // Skip CRLF
                line_start = ptr;
            } else {
                ptr++;
            }
        }

        if (lines.empty()) {
            std::cerr << "No request lines found" << std::endl;
            return;
        }

        auto b = std::chrono::high_resolution_clock::now();
        auto _duration = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a);
        std::cerr << "Parse request lines: " << _duration.count() << " ns" << std::endl;

        a = std::chrono::high_resolution_clock::now();

        // Parse the first line (HTTP request line)
        {
            auto http_start_line = lines[0];
            auto line_data = http_start_line.data();
            auto line_end = line_data + http_start_line.size();
            
            // Find the first space (after method)
            const char* method_end = std::find(line_data, line_end, ' ');
            if (method_end == line_end) {
                std::cerr << "Invalid HTTP request line: no space after method" << std::endl;
                return;
            }
            
            std::string_view method_str(line_data, method_end - line_data);
            
            // Find the second space (after path)
            const char* path_start = method_end + 1;
            const char* path_end = std::find(path_start, line_end, ' ');
            if (path_end == line_end) {
                std::cerr << "Invalid HTTP request line: no space after path" << std::endl;
                return;
            }
            
            path = std::string_view(path_start, path_end - path_start);
            
            // Parse method (fixed comparison logic)
            if (method_str == "GET") {
                method = Method::GET;
            } else if (method_str == "POST") {
                method = Method::POST;
            } else if (method_str == "PUT") {
                method = Method::PUT;
            } else if (method_str == "HEAD") {
                method = Method::HEAD;
            } else if (method_str == "TRACE") {
                method = Method::TRACE;
            } else if (method_str == "DELETE") {
                method = Method::DELETE;
            } else if (method_str == "OPTIONS") {
                method = Method::OPTIONS;
            } else if (method_str == "CONNECT") {
                method = Method::CONNECT;
            } else {
                method = Method::UNKNOWN;
                std::cerr << "Unknown HTTP method: " << method_str << std::endl;
            }
        }
        
        b = std::chrono::high_resolution_clock::now();
        _duration = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a);
        std::cerr << "Parse request line: " << _duration.count() << " ns" << std::endl;


        a = std::chrono::high_resolution_clock::now();

        // Parse headers
        for (size_t i = 1; i < lines.size(); i++) {
            auto& line = lines[i];
            auto line_data = line.data();
            auto line_end = line_data + line.size();
            
            // Find the colon
            const char* colon = std::find(line_data, line_end, ':');
            if (colon == line_end) {
                // Invalid header, skip
                continue;
            }
            
            std::string_view name(line_data, colon - line_data);
            
            // Skip whitespace after colon
            const char* value_start = colon + 1;
            while (value_start < line_end && (*value_start == ' ' || *value_start == '\t')) {
                value_start++;
            }
            
            std::string_view value(value_start, line_end - value_start);
            headers.emplace_back(Header{name, value});
        }

        b = std::chrono::high_resolution_clock::now();
        _duration = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a);
        std::cerr << "Parse headers: " << _duration.count() << " ns" << std::endl;

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        std::cout << "Total Parsing took: " << duration.count() << "ns\n";
    }

    std::string create_response() const {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += "Hello, World!";
        return response;
    }

    void print() const {
        switch (method) {
            case Method::GET: std::cout << "Method: GET\n"; break;
            case Method::POST: std::cout << "Method: POST\n"; break;
            case Method::PUT: std::cout << "Method: PUT\n"; break;
            case Method::DELETE: std::cout << "Method: DELETE\n"; break;
            case Method::HEAD: std::cout << "Method: HEAD\n"; break;
            case Method::CONNECT: std::cout << "Method: CONNECT\n"; break;
            case Method::OPTIONS: std::cout << "Method: OPTIONS\n"; break;
            case Method::TRACE: std::cout << "Method: TRACE\n"; break;
            default: std::cout << "Method: UNKNOWN\n"; break;
        }

        std::cout << "Path: " << path << "\n";
        for (const auto& header : headers) {
            std::cout << "Header: " << header.name << ": " << header.value << "\n";
        }
    }
};

struct Connection
{
    int handle = -1;
    std::vector<char> buffer;
    std::vector<char> request_data;
    bool request_in_progress = false;

    Connection() : buffer(max_buffer_size) {}

    bool handle_read()
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (!request_in_progress) {
            request_data.clear();
            request_data.reserve(max_buffer_size);
            request_in_progress = true;
        }

        size_t current_request_size = request_data.size();
        int iterations = 0;
        bool request_completed = false;

        while (true) {
            // Get a chunk of the request
            int bytes_read = recv(handle, buffer.data(), buffer.size(), 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    // Connection closed by client
                    return true;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more data available right now
                    break;
                } else {
                    // Error
                    std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                    return true;
                }
            }

            current_request_size += bytes_read;
            // Check if the request is too large
            if (current_request_size > max_request_size) {
                std::cerr << "Request too large" << std::endl;
                return true;
            }

            // Append data to request buffer
            request_data.insert(request_data.end(), buffer.data(), buffer.data() + bytes_read);
            iterations++;

            // Create a temporary request object to check if the request is complete
            Request temp_request = Request::from_content(std::vector<char>(request_data));
            if (temp_request.check_complete()) {
                request_completed = true;
                break;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "handle_read took " << duration.count() << "us, " << iterations << " iterations\n";

        // Only process complete requests
        if (request_completed) {
            Request request = Request::from_content(std::move(request_data));
            request.parse();
            // request.print();

            // Send response
            std::string response = request.create_response();
            if (send(handle, response.data(), response.size(), 0) == -1) {
                std::cerr << "Error sending response: " << strerror(errno) << std::endl;
            }
            
            request_in_progress = false;
            return true; // Close connection after response (HTTP/1.0 behavior)
        }

        return false; // Keep connection open, waiting for more data
    }
};

void start_server()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "Error creating server socket: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    int port = 8080;
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1) {
        std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
    }
    
    set_nonblocking(server_socket);

    if (bind(server_socket, (const sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        std::cerr << "Error binding server socket: " << strerror(errno) << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SOMAXCONN) == -1)
    {
        std::cerr << "Error listening to server socket: " << strerror(errno) << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Listening on port " << port << std::endl;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Error creating epoll: " << strerror(errno) << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    epoll_event event = {};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        std::cerr << "Error adding server socket to epoll: " << strerror(errno) << std::endl;
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    std::array<Connection, max_connections> connections;
    std::unordered_map<int, size_t> socket_to_connection;

    constexpr size_t max_events = 1024;
    epoll_event events[max_events];
    
    while (true)
    {
        int num_events = epoll_wait(epoll_fd, events, max_events, -1);
        if (num_events == -1)
        {
            if (errno == EINTR) {
                // Interrupted system call, try again
                continue;
            }
            std::cerr << "Error waiting for events: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // Accept new connections
                while (true) {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
                    
                    if (client_socket == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more connections to accept
                            break;
                        }
                        std::cerr << "Error accepting client socket: " << strerror(errno) << std::endl;
                        continue;
                    }

                    // Find an available connection slot
                    size_t selected_connection = SIZE_MAX;
                    for (size_t j = 0; j < connections.size(); j++) {
                        if (connections[j].handle == -1) {
                            selected_connection = j;
                            break;
                        }
                    }
                    
                    if (selected_connection == SIZE_MAX) {
                        std::cerr << "Error: too many connections" << std::endl;
                        close(client_socket);
                        continue;
                    }

                    Connection& connection = connections[selected_connection];
                    set_nonblocking(client_socket);

                    epoll_event client_event = {};
                    client_event.events = EPOLLIN | EPOLLET;
                    client_event.data.fd = client_socket;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_event) == -1)
                    {
                        std::cerr << "Error adding client socket to epoll: " << strerror(errno) << std::endl;
                        close(client_socket);
                        continue;
                    }

                    connection.handle = client_socket;
                    connection.request_in_progress = false;
                    socket_to_connection[client_socket] = selected_connection;
                }
            }
            else
            {
                auto start = std::chrono::high_resolution_clock::now();
                int client_socket = events[i].data.fd;
                auto it = socket_to_connection.find(client_socket);
                
                if (it == socket_to_connection.end()) {
                    std::cerr << "Error: socket not found" << std::endl;
                    continue;
                }

                size_t connection_index = it->second;
                Connection& connection = connections[connection_index];
                
                bool should_close = false;
                
                if (events[i].events & EPOLLIN) {
                    should_close = connection.handle_read();
                }
                
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    should_close = true;
                }
                
                if (should_close) {
                    close(connection.handle);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection.handle, NULL);
                    socket_to_connection.erase(connection.handle);
                    connection.handle = -1;
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                std::cout << "Request took " << duration.count() << "us\n";
            }
        }
    }
    
    // Cleanup (though we never reach here in practice)
    close(server_socket);
    close(epoll_fd);
}

int main()
{
    start_server();
    return 0;
}