#pragma once

#include <iosfwd>

namespace ray {

constexpr double kEpsilon = 1.0e-6;
constexpr double kRayTMin = 1.0e-4;

struct Vec3 {
    double x;
    double y;
    double z;

    Vec3();
    Vec3(double x_value, double y_value, double z_value);

    double lengthSquared() const;
    double length() const;
    Vec3 normalized() const;
    bool isNearZero(double epsilon = kEpsilon) const;
};

using Color = Vec3;

Vec3 operator+(const Vec3& left, const Vec3& right);
Vec3 operator-(const Vec3& left, const Vec3& right);
Vec3 operator-(const Vec3& value);
Vec3 operator*(const Vec3& left, const Vec3& right);
Vec3 operator*(const Vec3& value, double scalar);
Vec3 operator*(double scalar, const Vec3& value);
Vec3 operator/(const Vec3& value, double scalar);
Vec3& operator+=(Vec3& left, const Vec3& right);
Vec3& operator-=(Vec3& left, const Vec3& right);
Vec3& operator*=(Vec3& value, double scalar);
Vec3& operator/=(Vec3& value, double scalar);
bool operator==(const Vec3& left, const Vec3& right);
bool operator!=(const Vec3& left, const Vec3& right);
std::ostream& operator<<(std::ostream& stream, const Vec3& value);

double dot(const Vec3& left, const Vec3& right);
Vec3 cross(const Vec3& left, const Vec3& right);
double length(const Vec3& value);
Vec3 normalize(const Vec3& value);
double clamp(double value, double min_value, double max_value);
Color clampColor(const Color& value,
                 double min_value = 0.0,
                 double max_value = 1.0);

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray();
    Ray(const Vec3& origin_value, const Vec3& direction_value);

    Vec3 at(double t) const;
};

}  // namespace ray
