#include "ray/math.hpp"

#include <cmath>

namespace ray {

Vec3::Vec3() : x(0.0), y(0.0), z(0.0) {}

Vec3::Vec3(double x_value, double y_value, double z_value)
    : x(x_value), y(y_value), z(z_value) {}

double Vec3::lengthSquared() const {
    return x * x + y * y + z * z;
}

double Vec3::length() const {
    return std::sqrt(lengthSquared());
}

Vec3 Vec3::normalized() const {
    return normalize(*this);
}

bool Vec3::isNearZero(double epsilon) const {
    return std::fabs(x) < epsilon &&
           std::fabs(y) < epsilon &&
           std::fabs(z) < epsilon;
}

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

double dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(const Vec3& left, const Vec3& right) {
    return Vec3(left.y * right.z - left.z * right.y,
                left.z * right.x - left.x * right.z,
                left.x * right.y - left.y * right.x);
}

double length(const Vec3& value) {
    return value.length();
}

Vec3 normalize(const Vec3& value) {
    const double len = value.length();
    if (len <= kEpsilon) {
        return Vec3();
    }
    return value / len;
}

}  // namespace ray
