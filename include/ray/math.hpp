#pragma once

#include <iosfwd>

namespace ray {

constexpr double kEpsilon = 1.0e-6;
// [INTV:EDGE] 레이-도형 교차 t값의 최소 허용치 — 교차점에서 반사/그림자 레이를 다시 쏠 때, 부동소수점
// 오차로 새 레이의 원점이 표면 바로 "밑"에서 시작해 같은 표면과 즉시 재교차(self-intersection)하는
// "shadow acne" 현상을 막는 하한선.
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

// [INTV:ARCH] Color를 별도 타입이 아니라 Vec3의 별칭으로 재사용(x=R, y=G, z=B) — 색상 연산자/수학
// 연산자를 중복 정의하지 않고 벡터 연산 인프라를 그대로 재활용한다.
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
