#include "ray/math.hpp"

#include <algorithm>
#include <cmath>
#include <ostream>

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

Vec3 operator*(const Vec3& left, const Vec3& right) {
    return Vec3(left.x * right.x, left.y * right.y, left.z * right.z);
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

Vec3& operator+=(Vec3& left, const Vec3& right) {
    left.x += right.x;
    left.y += right.y;
    left.z += right.z;
    return left;
}

Vec3& operator-=(Vec3& left, const Vec3& right) {
    left.x -= right.x;
    left.y -= right.y;
    left.z -= right.z;
    return left;
}

Vec3& operator*=(Vec3& value, double scalar) {
    value.x *= scalar;
    value.y *= scalar;
    value.z *= scalar;
    return value;
}

Vec3& operator/=(Vec3& value, double scalar) {
    value.x /= scalar;
    value.y /= scalar;
    value.z /= scalar;
    return value;
}

bool operator==(const Vec3& left, const Vec3& right) {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool operator!=(const Vec3& left, const Vec3& right) {
    return !(left == right);
}

std::ostream& operator<<(std::ostream& stream, const Vec3& value) {
    stream << value.x << ',' << value.y << ',' << value.z;
    return stream;
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

double clamp(double value, double min_value, double max_value) {
    return std::max(min_value, std::min(value, max_value));
}

Color clampColor(const Color& value, double min_value, double max_value) {
    return Color(clamp(value.x, min_value, max_value),
                 clamp(value.y, min_value, max_value),
                 clamp(value.z, min_value, max_value));
}

Ray::Ray() : origin(), direction(0.0, 0.0, 1.0) {}

Ray::Ray(const Vec3& origin_value, const Vec3& direction_value)
    : origin(origin_value), direction(direction_value) {}

Vec3 Ray::at(double t) const {
    return origin + direction * t;
}

}  // namespace ray
