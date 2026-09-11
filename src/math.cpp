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

// [INTV:EDGE] std::hypot(x,y,z)는 sqrt(x*x+y*y+z*z)와 수학적으로 같지만, 중간 제곱 항이 지나치게
// 커지거나 작아져 오버플로/언더플로가 나는 것을 표준 라이브러리가 내부적으로 방지해준다.
double Vec3::length() const {
    return std::hypot(x, y, z);
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

// [INTV:EDGE] 길이가 kEpsilon 이하인(정규화 불가능한) 벡터를 0/len으로 나누면 NaN/무한대가 나온다 —
// 그 직전에 영벡터로 대체해 이후 계산 전체가 NaN으로 오염되는 걸 막는다.
// - [TRAP] 이 가드 없이 재구현하면, 길이 0에 가까운 법선/방향 벡터 하나가 렌더러 파이프라인 어딘가에서
//   NaN을 만들고, 그 NaN이 이후 모든 산술 연산에 전파되어 "이미지가 전부 검게/이상하게 나오는데
//   원인을 추적하기 어려운" 디버깅하기 까다로운 버그로 이어진다.
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
