#pragma once

#include "render/OrbitCamera.h"
#include <algorithm>
#include <glm/glm.hpp>

// Axis-aligned bounding box.
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// Ray/AABB intersection, slab method. Mirrors RayBox() in scene.frag so CPU
// picking and GPU reflections agree on what the ray hits.
// The box is the intersection of three axis-aligned slabs; the ray is inside
// it between the last slab entry and the first slab exit.
inline bool IntersectRayAABB(const Ray& ray, const AABB& box, float& tHit) {
    glm::vec3 invDir = 1.0f / ray.dir;   // IEEE infinity handles axis-parallel rays
    glm::vec3 t0 = (box.min - ray.origin) * invDir;
    glm::vec3 t1 = (box.max - ray.origin) * invDir;
    glm::vec3 tNear = glm::min(t0, t1);
    glm::vec3 tFar = glm::max(t0, t1);
    float enter = std::max({tNear.x, tNear.y, tNear.z});
    float exit = std::min({tFar.x, tFar.y, tFar.z});
    if (exit < std::max(enter, 0.0f)) return false;
    tHit = enter > 0.0f ? enter : exit;
    return true;
}
