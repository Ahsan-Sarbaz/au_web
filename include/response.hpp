#pragma once
#include "server.hpp"
#include <string>

class Response
{
    friend class Server;
  public:
    Response() = default;

    static Response ok(const std::string& content)
    {
        Response response;
        response._content = content;
        response._status_code = 200;
        return response;
    }

    [[nodiscard]] inline const std::string& content() const
    {
        return _content;
    }

    [[nodiscard]] inline int status_code() const
    {
        return _status_code;
    }

  private:
    std::string to_http_response() const;

    std::string _content;
    int _status_code = 200;
};
