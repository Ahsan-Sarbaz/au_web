#include "server.hpp"
#include "request.hpp"
#include "response.hpp"
#include "router.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "Error getting socket flags: " << strerror(errno) << std::endl;
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "Error setting socket to non-blocking: " << strerror(errno) << std::endl;
    }
}

Server::Server(int port, Router& router) : port(port), server_socket(-1), epoll_fd(-1), router(router)
{
}

void Server::run()
{
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "Error creating server socket: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == -1)
    {
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

    constexpr size_t max_events = 1024;
    epoll_event events[max_events];

    while (true)
    {
        int num_events = epoll_wait(epoll_fd, events, max_events, -1);
        if (num_events == -1)
        {
            if (errno == EINTR)
            {
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
                while (true)
                {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);

                    if (client_socket == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // No more connections to accept
                            break;
                        }
                        std::cerr << "Error accepting client socket: " << strerror(errno) << std::endl;
                        continue;
                    }

                    // Find an available connection slot
                    size_t selected_connection = SIZE_MAX;
                    for (size_t j = 0; j < connections.size(); j++)
                    {
                        if (connections[j].handle == -1)
                        {
                            selected_connection = j;
                            break;
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

                if (it == socket_to_connection.end())
                {
                    std::cerr << "Error: socket not found" << std::endl;
                    continue;
                }

                size_t connection_index = it->second;
                Connection& connection = connections[connection_index];

                bool should_close = false;

                if (events[i].events & EPOLLIN)
                {
                    std::optional<Request> request_opt = connection.handle_request();

                    if (request_opt.has_value())
                    {
                        auto& request = request_opt.value();

                        RouteParams params;
                        auto node = router.find_route(request.method(), request.path().raw(), params);

                        if (node && node->handler)
                        {
                            request.set_params(params);
                            Response response = node->handler(request);
                            auto http_response = response.to_http_response();
                            if (send(connection.handle, http_response.data(), http_response.size(), 0) == -1)
                            {
                                std::cerr << "Error sending response: " << strerror(errno) << std::endl;
                            }
                        }
                        else
                        {
                            const char* response = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
                            if (send(connection.handle, response, strlen(response), 0) == -1)
                            {
                                std::cerr << "Error sending response: " << strerror(errno) << std::endl;
                            }
                        }
                    }

                    // NOTE(sarbaz) right now we always close the connections
                    should_close = true;
                }

                if (events[i].events & (EPOLLERR | EPOLLHUP)) { should_close = true; }

                if (should_close)
                {
                    close(connection.handle);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection.handle, NULL);
                    socket_to_connection.erase(connection.handle);
                    connection.handle = -1;
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                std::cout << "Request took " << duration.count() << "us" << std::endl;
            }
        }
    }

    // Cleanup (though we never reach here in practice)
    close(server_socket);
    close(epoll_fd);
}
