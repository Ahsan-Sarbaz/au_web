#include <cstddef>
#include <cstring>
#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


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

    int bind_result = bind(server_socket, (const sockaddr *)&server_address, sizeof(server_address));
    if (bind_result == -1)
    {
        std::cerr << "Error binding server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    int listen_result = listen(server_socket, SOMAXCONN);
    if (listen_result == -1)
    {
        std::cerr << "Error listening to server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Listening on port " << port << std::endl;

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Error creating epoll" << std::endl;
        exit(EXIT_FAILURE);
    }

    epoll_event event = {};
    event.events = EPOLLIN|EPOLLET;
    event.data.fd = server_socket;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        std::cerr << "Error adding server socket to epoll" << std::endl;
        exit(EXIT_FAILURE);
    }


    const int max_events = 1024;
    epoll_event events[max_events];
    while(true)
    {
        int num_events = epoll_wait(epoll_fd, events, max_events, -1);
        if (num_events == -1)
        {
            std::cerr << "Error waiting for events" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_socket) {
                int client_socket = accept(server_socket, NULL, NULL);
                if (client_socket == -1) {
                    std::cerr << "Error accepting client socket" << std::endl;
                    continue;
                }

                set_nonblocking(client_socket);

                epoll_event client_event = {};
                client_event.events = EPOLLIN|EPOLLET;
                client_event.data.fd = client_socket;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_event) == -1)
                {
                    std::cerr << "Error adding client socket to epoll" << std::endl;
                    close(client_socket);
                }
            }
            else 
            {
                int client_socket = events[i].data.fd;

                const int max_recv_size = 1024;
                char buffer[max_recv_size] = {};
                int bytes_read = recv(client_socket, buffer, max_recv_size - 1, 0);
                if (bytes_read == -1)
                {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        std::cerr << "Error reading from client socket" << std::endl;
                        close(client_socket);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                    }
                }
                else if (bytes_read == 0)
                {
                    close(client_socket);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                }
                else
                {

                    buffer[bytes_read] = '\0';
                    // std::cout << "Received: \n\n" << buffer << std::endl;
                    const char* ok = "HTTP/1.1 200 OK\r\n\r\n";
                    const size_t len = strlen(ok);
                    size_t bytes_written = write(client_socket, ok, len);
                    (void)bytes_written;
                    // close(client_socket);
                    // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                }
            }
        }
    }


    return 0;
}