#include "ray/material.hpp"

namespace ray {

Material::Material() : albedo(1.0, 1.0, 1.0) {}

Material::Material(const Color& color) : albedo(color) {}

}  // namespace ray
