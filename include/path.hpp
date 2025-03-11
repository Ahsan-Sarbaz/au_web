#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Path
{
    friend class Request;

  public:
    static Path from_string(const std::string_view& path);
    static std::string decode_percent(const std::string_view& encoded);

    [[nodiscard]] inline const std::vector<std::string>& get_segments() const
    {
        return segments;
    }

    [[nodiscard]] inline std::string get_query() const
    {
        return query;
    }
    
    void parse();
    void print() const;

  private:
    std::string whole;
    std::string query;                 // Decoded query
    std::unordered_map<std::string, std::string> parameters; // Decoded parameters
    std::vector<std::string> segments; // Decoded segments
};