#pragma once

#include "ray/renderer.hpp"

#include <iosfwd>
#include <string>

namespace ray {

void writePpm(const Image& image, std::ostream& output);
void writePpm(const Image& image, const std::string& path);
std::string checksumHex(const Image& image);

}  // namespace ray
