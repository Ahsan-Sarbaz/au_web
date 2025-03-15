#include "request.hpp"
#include "path.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

Request Request::from_content(std::vector<char>&& content, size_t header_size)
{
    Request request;
    request.content = std::move(content);
    request.header_size = header_size;
    request.body = std::span<char>(request.content.data() + header_size, request.content.size() - header_size);
    return request;
}

// this takes around (~3us)
void Request::parse()
{
    if (content.empty())
    {
        std::cerr << "Empty request content" << std::endl;
        return;
    }

    // Ensure content is null-terminated for safety
    if (content.back() != '\0') { content.push_back('\0'); }

    char* data = content.data();
    char* end = data + header_size;

    // Parse HTTP headers
    char* line_start = data;
    char* ptr = data;

    std::vector<std::string_view> lines;

    // Parse request lines (takes around ~1us)
    while (ptr < end - 1)
    {
        if (ptr[0] == '\r' && ptr[1] == '\n')
        {
            size_t line_length = ptr - line_start;

            if (line_length == 0)
            {
                // Empty line marks the end of headers
                ptr += 2; // Skip CRLF
                break;
            }

            lines.emplace_back(line_start, line_length);
            ptr += 2; // Skip CRLF
            line_start = ptr;
        }
        else { ptr++; }
    }

    if (lines.empty())
    {
        std::cerr << "No request lines found" << std::endl;
        return;
    }

    // Parse the first line (HTTP request line)
    {
        auto http_start_line = lines[0];
        auto line_data = http_start_line.data();
        auto line_end = line_data + http_start_line.size();

        // Find the first space (after method)
        const char* method_end = std::find(line_data, line_end, ' ');
        if (method_end == line_end)
        {
            std::cerr << "Invalid HTTP request line: no space after method" << std::endl;
            return;
        }

        std::string_view method_str(line_data, method_end - line_data);

        // Find the second space (after path)
        const char* path_start = method_end + 1;
        const char* path_end = std::find(path_start, line_end, ' ');
        if (path_end == line_end)
        {
            std::cerr << "Invalid HTTP request line: no space after path" << std::endl;
            return;
        }

        path = Path::from_string(std::string_view(path_start, path_end - path_start));
        path.parse();

        // Parse method (fixed comparison logic)
        if (method_str == "GET") { method = Method::GET; }
        else if (method_str == "POST") { method = Method::POST; }
        else if (method_str == "PUT") { method = Method::PUT; }
        else if (method_str == "HEAD") { method = Method::HEAD; }
        else if (method_str == "TRACE") { method = Method::TRACE; }
        else if (method_str == "DELETE") { method = Method::DELETE; }
        else if (method_str == "OPTIONS") { method = Method::OPTIONS; }
        else if (method_str == "CONNECT") { method = Method::CONNECT; }
        else
        {
            method = Method::UNKNOWN;
            std::cerr << "Unknown HTTP method: " << method_str << std::endl;
        }
    }

    // Parse headers
    for (size_t i = 1; i < lines.size(); i++)
    {
        auto& line = lines[i];
        auto line_data = line.data();
        auto line_end = line_data + line.size();

        // Find the colon
        const char* colon = std::find(line_data, line_end, ':');
        if (colon == line_end)
        {
            // Invalid header, skip
            continue;
        }

        std::string_view name(line_data, colon - line_data);

        // Skip whitespace after colon
        const char* value_start = colon + 1;
        while (value_start < line_end && (*value_start == ' ' || *value_start == '\t')) { value_start++; }

        std::string_view value(value_start, line_end - value_start);

        headers[name] = value;
    }
}

std::string Request::create_response() const
{
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += "Hello, World!";
    return response;
}

void Request::print() const
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

    path.print();
    for (const auto& header : headers) { std::cout << "Header: " << header.first << ": " << header.second << "\n"; }
}