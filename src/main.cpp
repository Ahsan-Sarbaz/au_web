#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

constexpr size_t max_connections = 1024;
constexpr size_t max_buffer_size = 1024;
int epoll_fd;

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

struct Connection
{
    int handle = -1;
    int bytes_read = 0;
    char* buffer;

    bool handle_read()
    {
        int bytes_read = recv(handle, buffer, max_buffer_size - 1, 0);
        if (bytes_read == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                std::cerr << "Error reading from client handle" << std::endl;
                return true; // Indicate to close
            }
            return false; // No action needed
        }
        else if (bytes_read == 0)
        {
            return true; // Client disconnected, indicate to close
        }
        else
        {
            const char* ok = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            ssize_t bytes_written = write(handle, ok, strlen(ok));
            if (bytes_written == -1)
            {
                std::cerr << "Error writing to client" << std::endl;
            }
            return true; // Response sent, indicate to close
        }
    }
};

int main()
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
    for (auto& conn : connections)
    {
        conn.buffer = new char[max_buffer_size];
    }

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
                connection.bytes_read = 0;
                socket_to_connection[client_socket] = selected_connection;
                next_connection = (selected_connection + 1) % connections.size();
            }
            else
            {
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
            }
        }
    }

    return 0;
}