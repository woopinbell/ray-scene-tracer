#include "ray/parser.hpp"

#include <cctype>
#include <cmath>
#include <limits>
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

[[maybe_unused]] void expectCount(const std::vector<std::string>& tokens,
                 std::size_t expected,
                 const std::string& source_name,
                 std::size_t line_number,
                 const std::string& form) {
    if (tokens.size() != expected) {
        std::ostringstream message;
        message << "expected " << form << " with " << expected - 1
                << " argument(s), got " << tokens.size() - 1;
        throw ParseError(source_name, line_number, message.str());
    }
}

double parseDoubleToken(const std::string& token,
                        const std::string& source_name,
                        std::size_t line_number,
                        const std::string& field_name) {
    try {
        std::size_t parsed = 0;
        const double value = std::stod(token, &parsed);
        if (parsed != token.size() || !std::isfinite(value)) {
            throw std::invalid_argument("not a finite number");
        }
        return value;
    } catch (const std::exception&) {
        throw ParseError(source_name,
                         line_number,
                         "invalid " + field_name + " value '" + token + "'");
    }
}

[[maybe_unused]] int parseIntToken(const std::string& token,
                  const std::string& source_name,
                  std::size_t line_number,
                  const std::string& field_name) {
    try {
        std::size_t parsed = 0;
        const long long value = std::stoll(token, &parsed);
        if (parsed != token.size() ||
            value < 1 ||
            value > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("not a positive int");
        }
        return static_cast<int>(value);
    } catch (const std::exception&) {
        throw ParseError(source_name,
                         line_number,
                         "invalid " + field_name + " value '" + token + "'");
    }
}

[[maybe_unused]] double parseRatio(const std::string& token,
                  const std::string& source_name,
                  std::size_t line_number,
                  const std::string& field_name) {
    const double value =
        parseDoubleToken(token, source_name, line_number, field_name);
    if (value < 0.0 || value > 1.0) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must be between 0.0 and 1.0");
    }
    return value;
}

[[maybe_unused]] double parsePositiveDouble(const std::string& token,
                           const std::string& source_name,
                           std::size_t line_number,
                           const std::string& field_name) {
    const double value =
        parseDoubleToken(token, source_name, line_number, field_name);
    if (value <= 0.0) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must be positive");
    }
    return value;
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
