#include "response.hpp"

std::string Response::to_http_response() const
{
    return "HTTP/1.1 " + std::to_string(_status_code) + "\r\nContent-Length: " + std::to_string(_content.size()) +
           "\r\nContent-Type: text/plain\r\n\r\n" + _content;
}
