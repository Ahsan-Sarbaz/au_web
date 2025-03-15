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

    [[nodiscard]] inline const std::string& raw() const
    {
        return _raw;
    }

    [[nodiscard]] inline const std::vector<std::string>& get_segments() const
    {
        return _segments;
    }

    [[nodiscard]] inline std::string get_query() const
    {
        return _query;
    }

    [[nodiscard]] inline const std::unordered_map<std::string, std::string>& get_parameters() const
    {
        return _parameters;
    }

    void parse();
    void print() const;

  private:
    std::string _raw;
    std::string _query;
    std::unordered_map<std::string, std::string> _parameters;
    std::vector<std::string> _segments;
};