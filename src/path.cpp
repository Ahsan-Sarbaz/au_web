#include "path.hpp"
#include <iostream>
#include <string_view>

Path Path::from_string(const std::string_view& path)
{
    Path p;
    p._raw = std::string(path);
    return p;
}

std::string Path::decode_percent(const std::string_view& encoded)
{
    std::string result;
    result.reserve(encoded.size()); // Optimize for common case

    for (size_t i = 0; i < encoded.size(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.size())
        {
            char hex[3] = {encoded[i + 1], encoded[i + 2], 0};
            char* endptr;
            unsigned int value = std::strtoul(hex, &endptr, 16);
            if (endptr == hex + 2)
            { // Valid hex
                result += static_cast<char>(value);
                i += 2; // Skip the two hex digits
            }
            else { return ""; }
        }
        else { result += encoded[i]; }
    }
    return result;
}

void Path::parse()
{
    if (_raw.empty()) return;

    auto start = _raw.data();
    auto end = _raw.data() + _raw.size();
    if (start[0] == '/') { start++; } // Skip leading /

    auto ptr = start;
    auto query_start = end;

    std::vector<std::string_view> raw_segments;
    std::string_view raw_query;

    raw_segments.clear();

    while (ptr < end)
    {
        if (*ptr == '/')
        {
            if (ptr > start) raw_segments.push_back(std::string_view(start, ptr - start));
            start = ptr + 1;
        }
        else if (*ptr == '?' && query_start == end)
        {
            if (ptr > start) raw_segments.push_back(std::string_view(start, ptr - start));
            query_start = ptr + 1;
            start = query_start;
        }
        ptr++;
    }

    if (start < end)
    {
        if (query_start < end)
            raw_query = std::string_view(query_start, end - query_start);
        else if (start > _raw.data() || _raw[0] == '/')
            raw_segments.push_back(std::string_view(start, end - start));
    }

    // Decode all components
    _segments.reserve(raw_segments.size());
    for (const auto& seg : raw_segments) { _segments.push_back(decode_percent(seg)); }
    if (!raw_query.empty()) _query = decode_percent(raw_query);

    if (!_query.empty()) {
        // Parse query parameters
        auto query_start = _query.data();
        auto query_end = _query.data() + _query.size();
        auto ptr = query_start;
        auto seperator = query_start;

        while (ptr < query_end) {
            if (*ptr == '=') {
                seperator = ptr;
            } else if (*ptr == '&') {
                if (seperator >= query_start && ptr > seperator) { // Valid key-value pair
                    std::string key = decode_percent(std::string_view(query_start, seperator - query_start));
                    std::string value = decode_percent(std::string_view(seperator + 1, ptr - seperator - 1));
                    if (!key.empty()) _parameters[key] = value;
                } else if (ptr > query_start) { // Key without value
                    std::string key = decode_percent(std::string_view(query_start, ptr - query_start));
                    if (!key.empty()) _parameters[key] = "";
                }
                query_start = ptr + 1;
                seperator = query_start;
            }
            ptr++;
        }

        // Handle the last pair
        if (ptr > query_start) {
            if (seperator >= query_start && ptr > seperator) { // Valid key-value pair
                std::string key = decode_percent(std::string_view(query_start, seperator - query_start));
                std::string value = decode_percent(std::string_view(seperator + 1, ptr - seperator - 1));
                if (!key.empty()) _parameters[key] = value;
            } else { // Key without value
                std::string key = decode_percent(std::string_view(query_start, ptr - query_start));
                if (!key.empty()) _parameters[key] = "";
            }
        }
    }
}

void Path::print() const
{
    std::cout << _raw << std::endl;
    if (!_segments.empty())
    {
        std::cout << "Segments: (" << _segments.size() << ")\n";
        for (const auto& segment : _segments) { std::cout << segment << std::endl; }
    }
    if (!_query.empty())
    {
        std::cout << "Query: " << _query << std::endl;
        std::cout << "Parameters: (" << _parameters.size() << ")\n";
        for (const auto& [key, value] : _parameters) { std::cout << key << ": " << value << std::endl; }
    }
}
