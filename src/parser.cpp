#include "ray/parser.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace ray {

namespace {

std::string makeParseMessage(const std::string& source_name,
                             std::size_t line_number,
                             const std::string& message) {
    std::ostringstream output;
    output << source_name;
    if (line_number > 0) {
        output << ':' << line_number;
    }
    output << ": " << message;
    return output.str();
}

[[maybe_unused]] std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

[[maybe_unused]] std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream input(line);
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

}  // namespace

ParseError::ParseError(const std::string& source_name,
                       std::size_t line_number,
                       const std::string& message)
    : std::runtime_error(
          makeParseMessage(source_name, line_number, message)),
      source_(source_name),
      line_(line_number) {}

const std::string& ParseError::source() const noexcept {
    return source_;
}

std::size_t ParseError::line() const noexcept {
    return line_;
}

}  // namespace ray
