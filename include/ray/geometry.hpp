#pragma once

#include "ray/accel.hpp"
#include "ray/material.hpp"

#include <optional>
#include <string>

namespace ray {

class Shape;

// [INTV:ARCH] t는 "광선 시작점 + t*방향"으로 맞은 지점을 표현하는 매개변수 — 여러 도형 중 t가 가장
// 작은 쪽이 카메라에서 가장 가까운(=실제로 보이는) 표면이라는 것이 레이트레이싱의 기본 가시성 판정 규칙.
struct HitRecord {
    double t;
    Vec3 point;
    Vec3 normal;
    Material material;
    const Shape* shape;
    // [INTV:EDGE] 광선이 표면 바깥(앞면)에서 들어왔는지 여부 — 법선을 항상 "광선이 들어온 쪽"으로
    // 뒤집어두는 관례(setFaceNormal)의 결과를 셰이딩 단계가 재사용할 수 있게 저장해둔다.
    bool frontFace;

    HitRecord();
    void setFaceNormal(const Ray& ray, const Vec3& outward_normal);
};

// [INTV:ARCH] 모든 도형(Sphere/Plane/Cylinder)이 상속하는 추상 기반 클래스 — intersect/bounds/
// typeName을 순수 가상으로 선언해 "도형이라면 반드시 이 셋을 구현해야 한다"는 계약을 강제한다.
class Shape {
public:
    explicit Shape(const Material& material_value = Material());
    // [INTV:EDGE] Scene이 Shape*(실제로는 자식 객체)를 다형적으로 삭제하므로 소멸자를 반드시 virtual로
    // 선언해야 한다 — 안 그러면 자식 클래스 소멸자가 호출되지 않는 정의되지 않은 동작이 된다.
    virtual ~Shape() = default;

    const Material& material() const;
    virtual bool intersect(const Ray& ray,
                           double t_min,
                           double t_max,
                           HitRecord& hit) const = 0;
    // [INTV:ARCH] std::optional<Aabb>: 무한 평면(Plane)처럼 유한한 경계 상자를 가질 수 없는 도형은
    // nullopt로 표현 — BVH 빌드 시 이런 도형은 별도 취급(가속 구조 밖에서 항상 검사)해야 한다는 신호.
    virtual std::optional<Aabb> bounds() const = 0;
    virtual std::string typeName() const = 0;

protected:
    Material material_;
};

class Sphere : public Shape {
public:
    Sphere(const Vec3& center_value,
           double radius_value,
           const Material& material_value);

    const Vec3& center() const;
    double radius() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 center_;
    double radius_;
};

class Plane : public Shape {
public:
    Plane(const Vec3& point_value,
          const Vec3& normal_value,
          const Material& material_value);

    const Vec3& point() const;
    const Vec3& normal() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 point_;
    Vec3 normal_;
};

class Cylinder : public Shape {
public:
    Cylinder(const Vec3& center_value,
             const Vec3& axis_value,
             double radius_value,
             double height_value,
             const Material& material_value);

    const Vec3& center() const;
    const Vec3& axis() const;
    double radius() const;
    double height() const;

    bool intersect(const Ray& ray,
                   double t_min,
                   double t_max,
                   HitRecord& hit) const override;
    std::optional<Aabb> bounds() const override;
    std::string typeName() const override;

private:
    Vec3 center_;
    Vec3 axis_;
    double radius_;
    double height_;
};

}  // namespace ray
