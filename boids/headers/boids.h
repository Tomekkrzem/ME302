#ifndef BOIDS_H
#define BOIDS_H

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

// Single boid state (position, velocity, orientation, limits)
struct Boid
{
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};

    glm::quat orientation{1, 0, 0, 0};

    float minSpeed = 5.0f;
    float maxSpeed = 12.0f;
    float maxForce = 10.0f;
    float turnSpeed = 10.0f;
};

// Tuning parameters for flocking + target/bounds behavior
struct BoidParams
{
    float neighborRadius = 4.0f;
    float separationRadius = 6.0f;

    float wSep = 20.2f;
    float wAlign = 0.9f;
    float wCoh = 0.3f;

    float wBounds = 0.9f;
    float boundsMargin = 2.5f;
    float lookAhead = 1.2f;

    float wOrbit = 1.2f;
    float wTarget = 0.6f;
    float arriveRadius = 1.0f;
    float targetMinDist = 1.5f;
};

// Model-space directions and corrections for orienting the mesh
static constexpr glm::vec3 MODEL_NOSE(0.0f, 1.0f, 0.0f);
static constexpr glm::vec3 MODEL_UP  (0.0f, 0.0f, 1.0f);
static constexpr glm::vec3 WORLD_UP  (0.0f, 0.0f, 1.0f);

// ------------------------------- HELPERS ---------------------------------

// Limit vector magnitude (returns v unchanged if already under maxMag)
static glm::vec3 clamp(const glm::vec3& v, float maxMag)
{
    float mag = glm::length(v);
    if (mag > maxMag && mag > 0.0f) {
        return v * (maxMag / mag);
    }
    return v;
}

// Cohesion: steer toward neighbors' average position
static glm::vec3 cohesion(const Boid& b, const glm::vec3& avgPos, const int countNeighbors)
{
    glm::vec3 center = avgPos / (float)countNeighbors;
    glm::vec3 desired = center - b.pos;
    float dist = glm::length(desired);

    if (dist < 1e-5f) {
        return glm::vec3(0.0f);
    }

    desired = (desired / dist) * b.maxSpeed;
    glm::vec3 sep = desired - b.vel;

    return clamp(sep, b.maxForce);
}

// Alignment: match neighbors' average velocity direction
static glm::vec3 alignment(const Boid& b, const glm::vec3& avgVel, const int countNeighbors)
{
    glm::vec3 align = avgVel / (float)countNeighbors;

    if (glm::dot(align,align) > 1e-6f){
        align = glm::normalize(align) * b.maxSpeed;
        return clamp(align - b.vel, b.maxForce);
    }

    return glm::vec3(0.0f);
}

// Separation: push away from nearby boids
static glm::vec3 separation(const Boid& b, glm::vec3& sep, const int countSeparation)
{
    sep /= (float)countSeparation;
    sep = clamp(sep, b.maxSpeed);
    sep = clamp(sep - b.vel, b.maxForce);

    return sep;
}

// Bounds: steer away from edges using a look-ahead point
static glm::vec3 boundaryAvoidance(const Boid& b, const BoidParams& params,
                                   const glm::vec3& minB, const glm::vec3& maxB)
{
    glm::vec3 velDir = (glm::dot(b.vel, b.vel) > 1e-6f) ? glm::normalize(b.vel) : glm::vec3(0,0,1);
    glm::vec3 futurePos = b.pos + velDir * b.maxSpeed * params.lookAhead;

    glm::vec3 push(0.0f);

    auto axisPush = [&](float p, float minV, float maxV, float& outAxis)
    {
        float dMin = p - minV;
        float dMax = maxV - p;

        if (dMin < params.boundsMargin)
        {
            float t = 1.0f - (dMin / params.boundsMargin);
            outAxis += (t * t);
        }
        if (dMax < params.boundsMargin)
        {
            float t = 1.0f - (dMax / params.boundsMargin);
            outAxis -= (t * t);
        }
    };

    axisPush(futurePos.x, minB.x, maxB.x, push.x);
    axisPush(futurePos.y, minB.y, maxB.y, push.y);
    axisPush(futurePos.z, minB.z, maxB.z, push.z);

    if (glm::dot(push, push) < 1e-6f)
        return glm::vec3(0.0f);

    glm::vec3 desired = glm::normalize(push) * b.maxSpeed;

    glm::vec3 steer = desired - b.vel;
    return clamp(steer, b.maxForce);
}

// Arrive: steer toward target, slow down near it, optional "dead zone" distance
static glm::vec3 arrive(const Boid& b, const glm::vec3& target, const BoidParams& params)
{
    glm::vec3 toTarget = target - b.pos;
    float dist = glm::length(toTarget);

    if (dist < 1e-5f) {
        return glm::vec3(0.0f);
    }

    if (params.targetMinDist > 0.0f && dist < params.targetMinDist) {
        return glm::vec3(0.0f);
    }

    glm::vec3 dir = toTarget / dist;

    float desiredSpeed = b.maxSpeed;
    if (dist < params.arriveRadius) {
        desiredSpeed = b.maxSpeed * (dist / params.arriveRadius);
    }

    glm::vec3 desiredVel = dir * desiredSpeed;

    glm::vec3 steer = desiredVel - b.vel;
    return clamp(steer, b.maxForce);
}

// Orbit: apply tangential steering around the target
static glm::vec3 orbit(const Boid& b, const glm::vec3& target, float strength)
{
    glm::vec3 to = target - b.pos;
    if (glm::dot(to,to) < 1e-6f) {
        return glm::vec3(0.0f);
    }

    glm::vec3 dir = glm::normalize(to);

    glm::vec3 axis = WORLD_UP;
    if (std::abs(glm::dot(dir, axis)) > 0.98f){
        axis = glm::vec3(0, 0, 1);
    }

    glm::vec3 tangent = glm::cross(axis, dir);
    float t2 = glm::dot(tangent, tangent);

    if (t2 < 1e-6f) {
        return glm::vec3(0.0f);
    }

    tangent *= (1.0f / std::sqrt(t2));

    glm::vec3 desired = tangent * b.maxSpeed;

    glm::vec3 steer = desired - b.vel;
    return clamp(steer, b.maxForce) * strength;
}

// Quaternion rotation taking one direction vector to another
static glm::quat quatFromTo(const glm::vec3& from, const glm::vec3& to)
{
    float f2 = glm::dot(from, from);
    float t2 = glm::dot(to, to);
    if (f2 < 1e-12f || t2 < 1e-12f)
        return glm::quat(1, 0, 0, 0);

    glm::vec3 f = from * glm::inversesqrt(f2);
    glm::vec3 t = to   * glm::inversesqrt(t2);

    float cosTheta = glm::dot(f, t);
    cosTheta = glm::clamp(cosTheta, -1.0f, 1.0f);

    if (cosTheta > 1.0f - 1e-6f)
        return glm::quat(1, 0, 0, 0);

    if (cosTheta < -1.0f + 1e-6f)
    {
        glm::vec3 axis = glm::cross(glm::vec3(1,0,0), f);
        if (glm::dot(axis, axis) < 1e-8f)
            axis = glm::cross(glm::vec3(0,1,0), f);

        axis = glm::normalize(axis);
        return glm::angleAxis(glm::pi<float>(), axis);
    }

    glm::vec3 axis = glm::cross(f, t);

    float s = std::sqrt((1.0f + cosTheta) * 2.0f);
    float invS = 1.0f / s;

    return glm::normalize(glm::quat(
        0.5f * s,
        axis.x * invS,
        axis.y * invS,
        axis.z * invS
    ));
}

// -------------------------- BOID UPDATE FUNCTIONS ------------------------

// Combine all steering forces for one boid
static glm::vec3 computeBoidSteer(const Boid& b, const std::vector<Boid>& boids, const BoidParams& params,
                                  const glm::vec3& minB, const glm::vec3& maxB, const glm::vec3& targetPos)
{
    glm::vec3 sep(0.0f);
    glm::vec3 avgPos(0.0f);
    glm::vec3 avgVel(0.0f);

    int countNeighbor = 0;
    int countSeparation = 0;

    // Gather neighbor stats
    for (const Boid& boid : boids)
    {
        if (&boid == &b) {
            continue;
        }

        glm::vec3 offset = boid.pos - b.pos;
        float d = glm::length(offset);
        if (d < 1e-5f) {
            continue;
        }

        if (d < params.neighborRadius) {
            avgPos += boid.pos;
            avgVel += boid.vel;
            countNeighbor++;
        }

        if (d < params.separationRadius) {
            sep -= (offset / d) * (1.0f / d);
            countSeparation++;
        }
    }

    // Weighted sum of behaviors
    glm::vec3 steer(0.0f);

    if (countSeparation > 0) {
        steer += params.wSep * separation(b, sep, countSeparation);
    }

    if (countNeighbor > 0) {
        steer += params.wAlign * alignment(b, avgVel, countNeighbor);
        steer += params.wCoh * cohesion(b, avgPos, countNeighbor);
    }

    steer += params.wTarget * arrive(b, targetPos, params);
    steer += params.wBounds * boundaryAvoidance(b, params, minB, maxB);
    steer += params.wOrbit * orbit(b, targetPos, 1.0f);

    return clamp(steer, b.maxForce);
}

// Integrate velocity/position with speed limits
static void integrateBoid(Boid& b, const glm::vec3& acc, float dt)
{
    b.vel += acc * dt;

    float speed = glm::length(b.vel);

    if (speed > b.maxSpeed && speed > 0.0f)
        b.vel *= (b.maxSpeed / speed);

    speed = glm::length(b.vel);
    if (speed < b.minSpeed)
    {
        if (speed < 1e-6f)
            b.vel = glm::vec3(0, 0, b.minSpeed);
        else
            b.vel *= (b.minSpeed / speed);
    }

    b.pos += b.vel * dt;
}

// Signed angle around axis (used for twist correction)
static float signedAngleAroundAxis(const glm::vec3& a, const glm::vec3& b, const glm::vec3& axis)
{
    float s = glm::dot(axis, glm::cross(a, b));
    float c = glm::dot(a, b);
    return std::atan2(s, c);
}

// Update boid orientation to face velocity and keep "up" stable
static void updateOrientationVelocity(Boid& b)
{
    float v2 = glm::dot(b.vel, b.vel);
    if (v2 < 1e-8f) return;

    glm::vec3 fwd = b.vel * glm::inversesqrt(v2);

    glm::quat q = quatFromTo(MODEL_NOSE, fwd);

    glm::vec3 curUp = q * MODEL_UP;

    glm::vec3 curUpP = curUp - fwd * glm::dot(curUp, fwd);
    glm::vec3 desUpP = WORLD_UP - fwd * glm::dot(WORLD_UP, fwd);

    float cu2 = glm::dot(curUpP, curUpP);
    float du2 = glm::dot(desUpP, desUpP);

    if (cu2 > 1e-8f && du2 > 1e-8f)
    {
        curUpP *= 1.0f / std::sqrt(cu2);
        desUpP *= 1.0f / std::sqrt(du2);

        float ang = signedAngleAroundAxis(curUpP, desUpP, fwd);
        glm::quat twist = glm::angleAxis(ang, fwd);

        q = glm::normalize(twist * q);
    }

    b.orientation = q;
}

// Update all boids (compute accelerations, then integrate)
void updateBoids(std::vector<Boid>& boids, const BoidParams& params, float dt,
                 const glm::vec3& minB, const glm::vec3& maxB, const glm::vec3& targetPos)
{
    std::vector<glm::vec3> acc(boids.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < boids.size(); i++)
    {
        acc[i] = computeBoidSteer(boids[i], boids, params, minB, maxB, targetPos);
    }

    for (size_t i = 0; i < boids.size(); i++)
    {
        integrateBoid(boids[i], acc[i], dt);
        updateOrientationVelocity(boids[i]);
    }
}

// Build model matrix (translate * rotate * scale)
static glm::mat4 boidModelMatrix(const Boid& b, float scale = 1.0f)
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), b.pos);
    glm::mat4 R = glm::mat4_cast(b.orientation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    return T * R * S;
}

#endif
