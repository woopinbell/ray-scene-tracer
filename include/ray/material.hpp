#pragma once

#include "ray/math.hpp"

namespace ray {

enum class MaterialType {
    Diffuse,
    Metal
};

struct Material {
    Color albedo;
    MaterialType type;

    Material();
    explicit Material(
        const Color& color,
        MaterialType type_value = MaterialType::Diffuse);
};

}  // namespace ray
