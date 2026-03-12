#include "Physics.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void UpdatePhysics(RigidBody& body, float dt) {

    if (body.Grounded) {

        body.velocity.x *= FRICTION;
        body.velocity.z *= FRICTION;

        if (glm::abs(body.velocity.x) < 0.001f) body.velocity.x = 0.0f;
        if (glm::abs(body.velocity.z) < 0.001f) body.velocity.z = 0.0f;

    }

    // Apply Gravity
    body.acceleration.y += GRAVITY;
    
    // Integrate for Velocity
    body.velocity += body.acceleration * dt;

    // Integrate for Position
    body.position += body.velocity * dt;

    // Reset Acceleration
    body.acceleration = glm::vec3(0.0f);

    // Resolve Ground Collisions
    ResolveGroundCollision(body);

}

void ResolveGroundCollision(RigidBody& body) {
    float groundContact = body.radius - GROUND_SLOP;

    if (body.position.y <= groundContact) {
        body.position.y  = groundContact;
        if (body.velocity.y < 0.0f)
            body.velocity.y = 0.0f;
        body.Grounded = true;
    } else {
        body.Grounded = false;
    }
}

void ResolveSphereCollision(RigidBody& a, RigidBody& b) {
    glm::vec3 delta    = a.position - b.position;
    float     distance = glm::length(delta);
    float     minDist  = a.radius + b.radius;

    if (distance >= minDist || distance <= 0.0f) return;

    const float RESTITUTION = 0.2f;

    glm::vec3 normal    = glm::normalize(delta);
    float     overlap   = minDist - distance;
    float     totalMass = a.mass + b.mass;

    // Push apart based on mass ratio
    a.position += normal * overlap * (b.mass / totalMass);
    b.position -= normal * overlap * (a.mass / totalMass);

    // Only resolve if moving toward each other
    float relativeVel = glm::dot(a.velocity - b.velocity, normal);
    if (relativeVel >= 0.0f) return;

    float impulse = -(1.0f + RESTITUTION) * relativeVel / (1.0f/a.mass + 1.0f/b.mass);

    glm::vec3 impulseVec = impulse * normal;
    a.velocity += impulseVec / a.mass;   // a gets pushed along normal
    b.velocity -= impulseVec / b.mass;   // b gets pushed opposite normal
}


void ApplyForce(RigidBody& body, glm::vec3 force) {
    
    body.acceleration += force / body.mass;

}