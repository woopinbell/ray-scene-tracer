#pragma once

#include "ray/scene.hpp"

#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace ray {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& source_name,
               std::size_t line_number,
               const std::string& message);

    const std::string& source() const noexcept;
    std::size_t line() const noexcept;

private:
    std::string source_;
    std::size_t line_;
};

namespace parser {
Scene parseScene(std::istream& input,
                 const std::string& source_name = "<stream>");
}  // namespace parser

}  // namespace ray
