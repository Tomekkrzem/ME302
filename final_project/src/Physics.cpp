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
    
    float groundLevel = GROUND_Y + body.radius;

    if (body.position.y <= groundLevel) {
        body.position.y = groundLevel;

        if (glm::abs(body.velocity.y) < 0.1f) {
            body.velocity.y = 0.0f;
            body.Grounded = true;
        } else {
            body.velocity.y *= -RESTITUTION;
            body.Grounded = false;
        }

    } else {

        body.Grounded = false;
    
    }

}

void ResolveSphereCollision(RigidBody& a, RigidBody& b) {

    glm::vec3 delta = a.position - b.position;

    float distance = glm::length(delta);
    float minDist = a.radius + b.radius;

    if (distance < minDist && distance > 0.0f) {

        glm::vec3 normal = glm::normalize(delta);

        float overlap = minDist - distance;
        float totalMass = a.mass + b.mass;

        a.position += normal * overlap * (b.mass / totalMass);
        b.position -= normal * overlap * (a.mass / totalMass);

        float aVel = glm::dot(a.velocity, normal);
        float bVel = glm::dot(b.velocity, normal);

        float aNewVel = (aVel * (a.mass - b.mass) + 2.0f * b.mass * bVel) / totalMass;
        float bNewVel = (bVel * (b.mass - a.mass) + 2.0f * a.mass * aVel) / totalMass;

        a.velocity += (aNewVel - aVel) * normal;
        b.velocity -= (bNewVel - bVel) * normal;

    }

}


void ApplyForce(RigidBody& body, glm::vec3 force) {
    
    body.acceleration += force / body.mass;

}