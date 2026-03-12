#pragma once
#include <glm/glm.hpp>
#include "Physics.h"
#include <algorithm>
#include <cfloat>
#include <fstream>
#include <filesystem>
#include <iomanip>


// --------------------- Genetic Information ---------------------

struct Genome {
    float size;        // 0.0 - 1.0  — larger body
    float speed;       // 0.0 - 1.0  — faster movement
    float predation;   // 0.0 - 1.0  — 0 = herbivore, 1 = carnivore
    float aggression;  // 0.0 - 1.0  — likelihood to attack nearby blobs
};

struct Phenotype {
    // Body
    float worldScale;       // visual + collision size
    float mass;             // heavier = harder to push, more damage on impact

    // Movement
    float moveForce;        // force applied when moving
    float maxSpeed;         // velocity cap

    // Energy
    float maxEnergy;        // total energy pool
    float energyCost;       // energy drained per second while moving
    float attackDamage;     // damage dealt to victim per attack hit
    float attackRange;      // world units — how close to attack

    // Perception
    float sightDist;        // how far the blob can detect food/prey

    // Diet
    float predationBias;    // 0 = seeks food only, 1 = seeks blobs only
    float aggression;       // chance to attack when prey is in range

    // Health
    float maxHealth;          // scales with size
    float starvationRate;     // hp/sec lost when energy == 0, scales with size

    // Visual
    glm::vec3 color;
};

Phenotype MakePhenotype(const Genome& g, float baseScale, float baseRadius);

Genome RandomGenome();

Genome Crossover(const Genome& a, const Genome& b);

Genome Mutate(const Genome& g, float mutationRate = 0.1f, float mutationStrength = 0.1f);

// --------------------- Food ---------------------

struct Food {
    glm::vec3 position;
    bool      active   = true;
    float     energy   = 30.0f; // energy restored when eaten
    float     respawnTimer  = 0.0f;   // counts down before becoming active
    float     respawnDelay  = 15.0f;  // seconds before reappearing
};

extern int gFoodCount;
extern std::vector<Food> gFood;
const float EAT_RADIUS  = 0.5f; // world units

void SpawnFood(float planeSize);

void RespawnFood(Food& f);

void UpdateFood(float dt);

// --------------------- Blobs ---------------------

struct LifetimeStats {
    float healthLost       = 0.0f;
    float energyGained     = 0.0f;
    float damageDealt      = 0.0f;
    float timeAlive        = 0.0f;
    int   foodEaten        = 0;
    int   killCount        = 0;
};

struct BlobInstance {
    Genome        genome;
    Phenotype     phenotype;
    RigidBody     body;
    LifetimeStats stats;
    glm::mat4     modelMatrix = glm::mat4(1.0f);
    glm::vec3     wanderDir   = glm::vec3(0.0f);
    float         wanderTimer = 0.0f;
    float         energy      = 100.0f;
    float         health      = 100.0f;
    bool          alive       = true;
    float         attackCooldown  = 0.0f;   // seconds until next attack is allowed
    float         decisionTimer   = 0.0f;
    int           lockedFoodIdx   = -1;
    int           lockedPreyIdx   = -1;
    int           lockedThreatIdx = -1;
    float hitFlashTimer = 0.0f;

    BlobInstance(Genome g, Phenotype p, glm::vec3 startPos, float mass, float radius)
        : genome(g), phenotype(p), body(startPos, mass, radius),
          energy(p.maxEnergy), health(p.maxHealth) {}
};

void UpdateAI(float dt, std::vector<BlobInstance>& blobs);

// --------------------- Evolution ---------------------

// Generation manager
struct Generation {
    int   number          = 0;
    float timer           = 0.0f;
    float duration        = 90.0f; // seconds per generation
    bool  running         = true;
};

extern Generation gGeneration;

float ComputeFitness(const BlobInstance& blob);

void  EvolveGeneration(std::vector<BlobInstance>& blobs,
                       int targetCount,
                       float worldRadius,
                       float baseScale);

// --------------------- Logging ---------------------

// Record for a single individual in a generation
struct IndividualRecord {
    int   generationNumber;
    int   blobIndex;
    bool  survived;
    float fitness;

    // Stats
    float timeAlive;
    float energyGained;
    float healthLost;
    float damageDealt;
    int   foodEaten;
    int   killCount;

    // Genome
    float gSize;
    float gSpeed;
    float gPredation;
    float gAggression;

    // Phenotype
    float worldScale;
    float moveForce;
    float maxEnergy;
    float energyCost;
    float attackDamage;
    float maxHealth;
};

// Summary of the best/average stats for a generation
struct GenerationRecord {
    int   number;
    int   survivors;
    int   totalBlobs;
    float bestFitness;
    float avgFitness;
    float avgSize;
    float avgSpeed;
    float avgPredation;
    float avgAggression;
    float avgTimeAlive;
    float avgEnergyGained;
    int   totalKills;
    int   totalFoodEaten;

    IndividualRecord bestIndividual;
    std::vector<IndividualRecord> individuals; // all blobs this generation
};

extern std::vector<GenerationRecord> gGenerationLog;

void ExportGenerationCSV(const GenerationRecord& rec, const std::string& folder = "gen_logs");

void ExportGenerationSummaryCSV(const std::vector<GenerationRecord>& log,
                                const std::string& filename = "gen_logs/generation_summary.csv");

void LogGeneration(const std::vector<BlobInstance>& blobs,
                   const std::vector<std::pair<float,int>>& scored,
                   int generationNumber);