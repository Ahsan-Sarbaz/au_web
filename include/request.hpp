#pragma once

#include "common.hpp"
#include "path.hpp"
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
#include <unordered_map>

typedef std::unordered_map<std::string_view, std::string_view> Headers;

class Request
{
    friend class Server;
    friend class Connection;
public:
    static Request from_content(std::vector<char>&& content, size_t header_size);

    void print() const;

    [[nodiscard]] inline const std::vector<char>& content() const { return _content; }
    [[nodiscard]] inline const Headers& headers() const { return _headers; }
    [[nodiscard]] inline const Path& path() const { return _path; }
    [[nodiscard]] inline const std::span<char> body() const { return _body; }
    [[nodiscard]] inline Method method() const { return _method; }
    [[nodiscard]] inline bool is_complete() const { return _is_complete; }
    [[nodiscard]] inline const RouteParams& params() const { return _params; }
    [[nodiscard]] inline RouteParams& params() { return _params; }

private:
    void parse();
    void set_params(const RouteParams& params) { _params = params; }

private:
    std::vector<char> _content;
    Headers _headers;
    Path _path;
    Method _method = Method::UNKNOWN;
    size_t _header_size = 0;
    std::span<char> _body;
    bool _is_complete = false;
    RouteParams _params;
};