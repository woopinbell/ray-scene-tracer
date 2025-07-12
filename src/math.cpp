#include "ray/math.hpp"

namespace ray {

Vec3::Vec3() : x(0.0), y(0.0), z(0.0) {}

Vec3::Vec3(double x_value, double y_value, double z_value)
    : x(x_value), y(y_value), z(z_value) {}

Vec3 operator+(const Vec3& left, const Vec3& right) {
    return Vec3(left.x + right.x, left.y + right.y, left.z + right.z);
}

Vec3 operator-(const Vec3& left, const Vec3& right) {
    return Vec3(left.x - right.x, left.y - right.y, left.z - right.z);
}

Vec3 operator-(const Vec3& value) {
    return Vec3(-value.x, -value.y, -value.z);
}

Vec3 operator*(const Vec3& value, double scalar) {
    return Vec3(value.x * scalar, value.y * scalar, value.z * scalar);
}

Vec3 operator*(double scalar, const Vec3& value) {
    return value * scalar;
}

Vec3 operator/(const Vec3& value, double scalar) {
    return Vec3(value.x / scalar, value.y / scalar, value.z / scalar);
}

}  // namespace ray
