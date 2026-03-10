#pragma once
#include <glm/glm.hpp>

// --------------------- Physics Constants ---------------------

const float GRAVITY         = -9.8f;
const float GROUND_Y        =  0.0f;
const float RESTITUTION     =  0.4f;
const float FRICTION        =  0.98f;

// --------------------- Rigid Body ---------------------

struct RigidBody {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;

    float mass;
    float radius;
    bool Grounded;

    RigidBody(glm::vec3 startPos, float mass, float radius):
        position(startPos),
        velocity(0.0f),
        acceleration(0.0f),
        mass(mass),
        radius(radius),
        Grounded(false) {}
};

// --------------------- Physics Functions ---------------------

/*
* Steps the Physics Simulation by dt Seconds
* 
*  @param body : RigidBody to Update
*  @param dt   : Time Step
*
*  @return void
*/
void UpdatePhysics(RigidBody& body, float dt);

/*
* Resolves Collisions Between Ground Plane and Body
* 
*  @param body : RigidBody to Update
*
*  @return void
*/
void ResolveGroundCollision(RigidBody& body);

/*
* Resolves Collisions Between Body A and Body B
* 
*  @param a : RigidBody A to Update
*  @param b : RigidBody B to Update
*
*  @return void
*/
void ResolveSphereCollision(RigidBody& a, RigidBody& b);

/*
* Apply a Force to the Body
* 
*  @param body  : RigidBody to Update
*  @param force : Force Vector to Apply 
*
*  @return void
*/
void ApplyForce(RigidBody& body, glm::vec3 force);