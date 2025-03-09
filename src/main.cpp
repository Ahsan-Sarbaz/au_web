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

constexpr size_t max_connections = 1024;
constexpr size_t max_buffer_size = 1024;
constexpr size_t max_request_size = 4 * 1024 * 1024;
int epoll_fd;

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
    CONNECT
};

struct Request
{
    std::vector<char> content;
    std::vector<Header> headers;
    std::vector<std::string_view> lines;
    std::string_view path;
    Method method = Method::GET;

    static Request from_content(const std::vector<char>&& content)
    {
        Request request;
        request.content = content;
        return request;
    }

    void parse()
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        bool end = false;
        char* line_start = content.data();
        char* body = line_start;
        while (!end)
        {
            size_t line_length = 0;
            char* ptr = line_start;

            while (*ptr != '\n')
            {
                line_length++;
                ptr++;
                if (*ptr == '\0')
                {
                    end = true;
                    break;
                }
            }
            ptr++;

            if (line_length > 0) lines.emplace_back(std::string_view(line_start, line_length));
            line_start = ptr;

            if (*ptr == '\r' && *(ptr + 1) == '\n')
            {
                end = true;
                body = ptr + 2;
            }
        }

        {
            auto http_start_line = lines[0];
            auto ptr = http_start_line.data();

            auto http_method = http_start_line.data();
            while (*ptr != ' ') ptr++;
            size_t http_method_length = ptr - http_method;
            ptr++;
            auto http_path = ptr;
            while (*ptr != ' ') ptr++;
            size_t http_path_length = ptr - http_path;
            ptr++;
            auto http_version = ptr;
            while (*ptr != '\r') ptr++;

            auto http_version_length = ptr - http_version;
            (void)http_version_length;

            if (http_method_length >= 3 && http_method_length <= 7)
            {
                if (http_method[0] == 'G' && http_method[1] == 'E' && http_method[2] == 'T' && http_method[3] == ' ')
                    method = Method::GET;
                else if (http_method[0] == 'P' && http_method[1] == 'O' && http_method[2] == 'S' &&
                         http_method[3] == 'T')
                    method = Method::POST;
                else if (http_method[0] == 'P' && http_method[1] == 'U' && http_method[2] == 'T' &&
                         http_method[3] == ' ')
                    method = Method::PUT;
                else if (http_method[0] == 'H' && http_method[1] == 'E' && http_method[2] == 'A' &&
                         http_method[3] == 'D')
                    method = Method::HEAD;
                else if (http_method[0] == 'T' && http_method[1] == 'R' && http_method[2] == 'A' &&
                         http_method[3] == 'C' && http_method[4] == 'E')
                    method = Method::TRACE;
                else if (http_method[0] == 'D' && http_method[1] == 'E' && http_method[2] == 'L' &&
                         http_method[3] == 'E' && http_method[4] == 'T' && http_method[5] == 'E')
                    method = Method::DELETE;
                else if (http_method[0] == 'O' && http_method[1] == 'P' && http_method[2] == 'T' &&
                         http_method[3] == 'I' && http_method[4] == 'O' && http_method[5] == 'N')
                    method = Method::OPTIONS;
                else if (http_method[0] == 'C' && http_method[1] == 'O' && http_method[2] == 'N' &&
                         http_method[3] == 'N' && http_method[4] == 'E' && http_method[5] == 'C' &&
                         http_method[6] == 'T')
                    method = Method::CONNECT;
            }

            if (http_path_length > 0) path = std::string(http_path, http_path_length);
        }

        if (lines.size() > 1)
        {
            for (size_t i = 1; i < lines.size(); i++)
            {
                auto line = lines[i];
                auto ptr = line.data();
                auto name_start = ptr;
                while (*ptr != ':') ptr++;
                size_t name_length = ptr - name_start;
                ptr++;
                auto value_start = ptr;
                while (*ptr != '\r') ptr++;
                size_t value_length = ptr - value_start;
                headers.emplace_back(
                    Header{std::string_view(name_start, name_length), std::string_view(value_start, value_length)});
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        std::cout << "Parsing took: " << duration.count() << "us\n";

        (void)body;
    }

    void print()
    {
        switch (method)
        {
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
        for (auto& header : headers) { std::cout << "Header: " << header.name << ": " << header.value << "\n"; }
        // std::cout << "Content: " << std::string(content.begin(), content.end()) << "\n";
    }
};

struct Connection
{
    int handle = -1;
    char* buffer;

    bool handle_read()
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<char> request_buffer;
        request_buffer.reserve(max_buffer_size);

        size_t current_request_size = 0;

        int iterations = 0;
        while (true)
        {
            // get a chunk of the request
            int bytes_read = recv(handle, buffer, max_buffer_size, 0);
            if (bytes_read <= 0)
            {
                if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            }

            current_request_size += bytes_read;
            // if the request is larger then 4 mb just say, NO!
            if (current_request_size > max_request_size)
            {
                std::cerr << "Request too large" << std::endl;
                return true; // Indicate to close
            }

            request_buffer.insert(request_buffer.end(), buffer, buffer + bytes_read);

            iterations++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "handle_read took " << duration.count() << "us, " << iterations << " iterations\n";

        Request request = Request::from_content(std::move(request_buffer));
        request.parse();
        request.print();

        return true;
    }
};

void start_server()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "Error creating server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    int port = 8080;
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    set_nonblocking(server_socket);

    if (bind(server_socket, (const sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        std::cerr << "Error binding server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SOMAXCONN) == -1)
    {
        std::cerr << "Error listening to server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Listening on port " << port << std::endl;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Error creating epoll" << std::endl;
        exit(EXIT_FAILURE);
    }

    epoll_event event = {};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        std::cerr << "Error adding server socket to epoll" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::array<Connection, max_connections> connections;
    for (auto& conn : connections) { conn.buffer = new char[max_buffer_size]; }

    size_t next_connection = 0;
    std::unordered_map<int, size_t> socket_to_connection;

    constexpr size_t max_events = 1024;
    epoll_event events[max_events];
    while (true)
    {
        int num_events = epoll_wait(epoll_fd, events, max_events, -1);
        if (num_events == -1)
        {
            std::cerr << "Error waiting for events" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                int client_socket = accept(server_socket, NULL, NULL);
                if (client_socket == -1)
                {
                    std::cerr << "Error accepting client socket" << std::endl;
                    continue;
                }

                // Find an available slot, wrapping around if needed
                size_t selected_connection = SIZE_MAX;
                for (size_t j = next_connection; j < connections.size(); j++)
                {
                    if (connections[j].handle == -1)
                    {
                        selected_connection = j;
                        break;
                    }
                }
                if (selected_connection == SIZE_MAX)
                {
                    for (size_t j = 0; j < next_connection; j++)
                    {
                        if (connections[j].handle == -1)
                        {
                            selected_connection = j;
                            break;
                        }
                    }
                }
                if (selected_connection == SIZE_MAX)
                {
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
                    std::cerr << "Error adding client socket to epoll" << std::endl;
                    close(client_socket);
                    continue;
                }

                connection.handle = client_socket;
                socket_to_connection[client_socket] = selected_connection;
                next_connection = (selected_connection + 1) % connections.size();
            }
            else
            {
                auto start = std::chrono::high_resolution_clock::now();
                int client_socket = events[i].data.fd;
                if (socket_to_connection.find(client_socket) == socket_to_connection.end())
                {
                    std::cerr << "Error: socket not found" << std::endl;
                    continue;
                }

                size_t connection_index = socket_to_connection[client_socket];
                Connection& connection = connections[connection_index];
                if (connection.handle_read())
                {
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
}

int main()
{
    start_server();
    return 0;
}