#include "AI.h"
#include <iostream>
#include <algorithm>
#include <cfloat>
#include <fstream>
#include <filesystem>
#include <iomanip>

Generation                gGeneration;
std::vector<Food>         gFood;
std::vector<GenerationRecord> gGenerationLog;
extern float gP_Size;
int gFoodCount = 1000;

// --------------------- Phenotype ---------------------

Phenotype MakePhenotype(const Genome& g, float baseScale, float baseRadius) {
    Phenotype p;

    // Body
    p.worldScale = baseScale * (0.5f + g.size * 2.0f);
    p.mass       = 0.5f + g.size * 3.0f;

    // Movement — large blobs are slower
    float sizeSpeedPenalty = 1.0f - (g.size * 0.6f);
    float rawSpeed         = g.speed * sizeSpeedPenalty;
    p.moveForce = 0.5f + rawSpeed * 5.0f;
    p.maxSpeed  = 1.0f + rawSpeed * 8.0f;

    // Health — larger blobs have more HP but starve faster
    p.maxHealth      = 50.0f  + g.size * 150.0f;
    p.starvationRate = 3.0f   + g.size * 15.0f;

    // Energy — larger blobs have bigger tanks but higher upkeep
    p.maxEnergy  = 80.0f + g.size * 150.0f;
    float sizeCost     = g.size  * 3.0f;
    float speedCost    = g.speed * 4.0f;
    float synergyCost  = g.size  * g.speed * 6.0f;
    float predationCost = g.predation * 4.0f;  // add this
    p.energyCost = sizeCost + speedCost + synergyCost + predationCost + 2.0f;


    // Attack — bigger blobs hit harder
    p.attackDamage = 5.0f  + g.size * 30.0f;
    p.attackRange = p.worldScale * 4.0f;

    // Perception — predators see further, small blobs see further
    p.sightDist = 4.0f + g.predation * 12.0f
                       + (1.0f - g.size) * 4.0f;

    // Diet
    p.predationBias = g.predation;
    p.aggression    = g.aggression;

    // Color: red = size, green = herbivore, blue = speed
    p.color = glm::vec3(
        0.1f + g.size      * 0.9f,
        0.6f - g.predation * 0.6f,
        0.1f + g.speed     * 0.9f
    );

    return p;
}

// --------------------- Genome ---------------------

Genome RandomGenome() {
    auto rf = [](float base, float variance) {
        float v = base + ((rand() % 100) / 100.0f - 0.5f) * variance;
        return glm::clamp(v, 0.0f, 1.0f);
    };
    return {
        rf(0.3f, 0.1f),   // size       — tight cluster around 0.3
        rf(0.3f, 0.1f),   // speed      — tight cluster around 0.3
        rf(0.1f, 0.1f),   // predation  — low, small variance
        rf(0.1f, 0.1f),   // aggression — low, small variance
    };
}

Genome Crossover(const Genome& a, const Genome& b) {
    auto pick = [](float x, float y) {
        return (rand() % 2 == 0) ? x : y;
    };
    return {
        pick(a.size,       b.size),
        pick(a.speed,      b.speed),
        pick(a.predation,  b.predation),
        pick(a.aggression, b.aggression),
    };
}

Genome Mutate(const Genome& g, float mutationRate, float mutationStrength) {
    auto mutate = [&](float gene) {
        if ((rand() % 100) / 100.0f < mutationRate) {
            float delta = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * mutationStrength;
            return glm::clamp(gene + delta, 0.0f, 1.0f);
        }
        return gene;
    };
    return {
        mutate(g.size),
        mutate(g.speed),
        mutate(g.predation),
        mutate(g.aggression),
    };
}

// --------------------- Food ---------------------

void SpawnFood(float planeSize) {
    gFood.clear();
    float half = planeSize * 0.9f; // slight inset from edge
    for (int i = 0; i < gFoodCount; i++) {
        Food f;
        f.position = glm::vec3(
            ((rand() % 2000) / 1000.0f - 1.0f) * (gP_Size * 0.9f),
            0.1f,
            ((rand() % 2000) / 1000.0f - 1.0f) * (gP_Size * 0.9f)
        );
        gFood.push_back(f);
    }
}

void RespawnFood(Food& f) {
    f.active       = false;
    f.respawnTimer = f.respawnDelay;
}

void UpdateFood(float dt) {
    for (auto& food : gFood) {
        if (food.active) continue;
        food.respawnTimer -= dt;
        if (food.respawnTimer <= 0.0f) {
            food.position = glm::vec3(
                ((rand() % 2000) / 1000.0f - 1.0f) * (gP_Size * 0.9f),
                0.1f,
                ((rand() % 2000) / 1000.0f - 1.0f) * (gP_Size * 0.9f)
            );
            food.active = true;
        }
    }
}

// --------------------- AI Helpers ---------------------

// Returns index of nearest visible food, or -1
static int FindNearestFood(const BlobInstance& blob) {
    int   bestIdx  = -1;
    float bestDist = FLT_MAX;
    for (int fi = 0; fi < (int)gFood.size(); fi++) {
        if (!gFood[fi].active) continue;
        glm::vec3 delta = gFood[fi].position - blob.body.position;
        delta.y = 0.0f;
        float dist = glm::length(delta);
        if (dist < blob.phenotype.sightDist && dist < bestDist) {
            bestDist = dist;
            bestIdx  = fi;
        }
    }
    return bestIdx;
}

// Returns index of nearest huntable prey, or -1
static int FindNearestPrey(const BlobInstance& blob, int bi,
                            const std::vector<BlobInstance>& blobs) {
    // Nearly pure herbivores never hunt
    if (blob.phenotype.predationBias <= 0.05f) return -1;

    int   bestIdx  = -1;
    float bestDist = FLT_MAX;
    for (int pi = 0; pi < (int)blobs.size(); pi++) {
        if (pi == bi || !blobs[pi].alive) continue;

        // Attack anything meaningfully smaller, or same-size if the prey is starving
        bool isSmaller  = blobs[pi].body.radius < blob.body.radius * 1.2f;
        bool isStarving = blobs[pi].energy < blob.phenotype.maxEnergy * 0.3f;
        if (!isSmaller && !isStarving) continue;

        glm::vec3 delta = blobs[pi].body.position - blob.body.position;
        delta.y = 0.0f;
        float dist = glm::length(delta);
        if (dist < blob.phenotype.sightDist && dist < bestDist) {
            bestDist = dist;
            bestIdx  = pi;
        }
    }
    return bestIdx;
}

// Returns index of nearest visible predator threatening this blob, or -1
static int FindNearestThreat(const BlobInstance& blob, int bi,
                              const std::vector<BlobInstance>& blobs) {
    int   bestIdx  = -1;
    float bestDist = FLT_MAX;
    for (int pi = 0; pi < (int)blobs.size(); pi++) {
        if (pi == bi || !blobs[pi].alive) continue;

        const BlobInstance& other = blobs[pi];

        // A threat is a predator that is larger than us and hunting
        bool isPredator = other.phenotype.predationBias > 0.3f;
        bool isLarger   = other.body.radius > blob.body.radius;
        if (!isPredator || !isLarger) continue;

        glm::vec3 delta = other.body.position - blob.body.position;
        delta.y = 0.0f;
        float dist = glm::length(delta);

        // Prey uses its own sight distance to detect threats
        if (dist < blob.phenotype.sightDist && dist < bestDist) {
            bestDist = dist;
            bestIdx  = pi;
        }
    }
    return bestIdx;
}

// --------------------- Main AI Update ---------------------

void UpdateAI(float dt, std::vector<BlobInstance>& blobs) {
    for (int bi = 0; bi < (int)blobs.size(); bi++) {
        BlobInstance& blob = blobs[bi];
        if (!blob.alive) continue;

        blob.stats.timeAlive += dt;

        // --- Drain attack cooldown ---
        if (blob.attackCooldown > 0.0f)
            blob.attackCooldown -= dt;

        // --- Energy drain ---
        glm::vec3 hVel = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
        if (glm::length(hVel) > 0.1f)
            blob.energy -= blob.phenotype.energyCost * dt;
        blob.energy -= 1.0f * dt; // passive drain
        blob.energy  = std::max(blob.energy, 0.0f);

        // --- Starvation ---
        if (blob.energy <= 0.0f) {
            float dmg         = blob.phenotype.starvationRate * dt;
            blob.health      -= dmg;
            blob.stats.healthLost += dmg;
        }
        blob.health = std::max(blob.health, 0.0f);
        if (blob.health <= 0.0f) {
            blob.alive = false;
            continue;
        }

        // --- Perception ---
        int foodIdx   = FindNearestFood(blob);
        int preyIdx   = FindNearestPrey(blob, bi, blobs);
        int threatIdx = FindNearestThreat(blob, bi, blobs);

        // --- Behavior priority: flee > hunt > forage > wander ---
        if (threatIdx != -1) {
            // FLEE — run directly away from the threat
            glm::vec3 toThreat = blobs[threatIdx].body.position - blob.body.position;
            toThreat.y = 0.0f;
            blob.wanderDir = -glm::normalize(toThreat); // opposite direction

            // Reset wander timer so fleeing isn't interrupted
            blob.wanderTimer = 1.0f;

        } else if (preyIdx != -1 &&
                   (blob.phenotype.predationBias > 0.3f || foodIdx == -1)) {
            // HUNT — chase prey (threshold lowered from 0.5 to 0.3)
            glm::vec3 toPrey = blobs[preyIdx].body.position - blob.body.position;
            toPrey.y = 0.0f;
            blob.wanderDir = glm::normalize(toPrey);

            // Attack if close enough, cooldown has expired, and aggression roll passes
            if (glm::length(toPrey) < blob.phenotype.attackRange &&
                blob.attackCooldown <= 0.0f &&
                ((float)(rand() % 100) / 100.0f) < blob.phenotype.aggression) {

                // FIX: full damage per hit, not scaled by dt — use attackCooldown to rate-limit
                float dmg = blob.phenotype.attackDamage;
                blobs[preyIdx].health          -= dmg;
                blobs[preyIdx].hitFlashTimer = 0.2f;
                blobs[preyIdx].health           = std::max(blobs[preyIdx].health, 0.0f);
                blobs[preyIdx].stats.healthLost += dmg;
                blob.stats.damageDealt          += dmg;
                blob.energy                      = std::min(blob.energy + dmg * 0.5f,
                                                            blob.phenotype.maxEnergy);
                blob.stats.energyGained         += dmg * 0.5f;

                // Rate-limit to ~2 attacks per second
                blob.attackCooldown = 0.5f;

                if (blobs[preyIdx].health <= 0.0f) {
                    blobs[preyIdx].alive = false;
                    blob.stats.killCount++;
                }
            }

        } else if (foodIdx != -1) {
            // FORAGE — move toward food
            glm::vec3 toFood = gFood[foodIdx].position - blob.body.position;
            toFood.y = 0.0f;
            blob.wanderDir = glm::normalize(toFood);

            if (glm::length(toFood) < EAT_RADIUS) {
                float gained         = gFood[foodIdx].energy;
                blob.energy          = std::min(blob.energy + gained, blob.phenotype.maxEnergy);
                blob.stats.energyGained += gained;
                blob.stats.foodEaten++;
                RespawnFood(gFood[foodIdx]);
            }

        } else {
            // WANDER
            blob.wanderTimer -= dt;
            if (blob.wanderTimer <= 0.0f) {
                float angle      = (rand() % 180) * (3.14159f / 180.0f);
                blob.wanderDir   = glm::vec3(cos(angle), 0.0f, sin(angle));
                blob.wanderTimer = 2.0f + (rand() % 200) * 0.01f;
            }
        }

        // --- Apply movement ---
        if (blob.body.Grounded) {
            ApplyForce(blob.body, blob.wanderDir * blob.phenotype.moveForce);

            // Unstick: kick blob if it should be moving but isn't
            glm::vec3 hv = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
            if (glm::length(hv) < 0.05f && glm::length(blob.wanderDir) > 0.1f) {
                blob.body.velocity.x = blob.wanderDir.x * 0.3f;
                blob.body.velocity.z = blob.wanderDir.z * 0.3f;
            }
        }

        // --- Boundary push ---
        float BOUNDARY = gP_Size;

        float px = blob.body.position.x;
        float pz = blob.body.position.z;

        if (glm::abs(px) > BOUNDARY || glm::abs(pz) > BOUNDARY) {
            glm::vec3 pushDir = glm::vec3(0.0f);
            if (glm::abs(px) > BOUNDARY) pushDir.x = -glm::sign(px);
            if (glm::abs(pz) > BOUNDARY) pushDir.z = -glm::sign(pz);
            pushDir = glm::normalize(pushDir);

            ApplyForce(blob.body, pushDir * 8.0f);
            blob.wanderDir = pushDir;
        }
    }
}

// --------------------- Fitness ---------------------

float ComputeFitness(const BlobInstance& blob) {
    float fitness = 0.0f;

    if (blob.alive)                    fitness += 300.0f;        // reduced from 500 — survival alone shouldn't dominate
    fitness += blob.stats.timeAlive        *   2.0f;
    fitness += blob.stats.energyGained     *   1.5f;
    fitness += blob.stats.foodEaten        *  15.0f;             // reduced from 20
    float killReward = std::min(blob.stats.killCount, 3) * 150.0f   // first 3 kills full value
                 + std::max(0, blob.stats.killCount - 3) * 50.0f; 
    fitness += killReward;
    fitness += blob.stats.damageDealt      *   2.0f;             // increased from 0.5 — reward aggression even without kills
    fitness -= blob.stats.healthLost       *   0.5f;             // reduced from 1.0 — less penalty for taking hits

    // Bonus fitness for Herbivore in predator rich environment
    float herbivoreBonus = (1.0f - blob.genome.predation) * blob.stats.timeAlive * 1.5f;
    fitness += herbivoreBonus;

    // Penalize predators that never hunted
    if (blob.stats.killCount == 0 && blob.genome.predation > 0.3f)
        fitness -= blob.genome.predation * 30.0f;                // reduced from 50 — don't over-punish early predators

    return std::max(fitness, 0.0f);
}

// --------------------- Evolution ---------------------

void EvolveGeneration(std::vector<BlobInstance>& blobs,
                      int targetCount, float worldRadius, float baseScale) {
    gGeneration.number++;

    // Score and sort
    std::vector<std::pair<float, int>> scored;
    scored.reserve(blobs.size());
    for (int i = 0; i < (int)blobs.size(); i++)
        scored.push_back({ ComputeFitness(blobs[i]), i });

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    LogGeneration(blobs, scored, gGeneration.number);
    ExportGenerationCSV(gGenerationLog.back());
    ExportGenerationSummaryCSV(gGenerationLog);

    const GenerationRecord& latest = gGenerationLog.back();
    std::cout << "=== GEN " << latest.number
              << " | survivors=" << latest.survivors << "/" << latest.totalBlobs
              << " | best="      << latest.bestFitness
              << " | avg="       << latest.avgFitness
              << std::endl;

    // Select top 50% as parents
    int parentCount = std::max(2, (int)scored.size() / 2);
    std::vector<int>   parentIndices;
    std::vector<float> parentFitnesses;
    parentIndices.reserve(parentCount);
    parentFitnesses.reserve(parentCount);

    for (int i = 0; i < parentCount; i++) {
        parentIndices.push_back(scored[i].second);
        parentFitnesses.push_back(scored[i].first);
    }

    float totalFitness = 0.0f;
    for (float f : parentFitnesses) totalFitness += f;

    // Fitness-proportionate parent selection
    auto selectParent = [&]() -> int {
        if (totalFitness <= 0.0f)
            return parentIndices[rand() % parentIndices.size()];
        float r   = ((rand() % 10000) / 10000.0f) * totalFitness;
        float acc = 0.0f;
        for (int i = 0; i < (int)parentIndices.size(); i++) {
            acc += parentFitnesses[i];
            if (r <= acc) return parentIndices[i];
        }
        return parentIndices.back();
    };

    std::vector<BlobInstance> nextGen;
    nextGen.reserve(targetCount);

    // Elitism — carry top 2 directly into next generation
    for (int i = 0; i < std::min(2, parentCount); i++) {
        auto& elite        = blobs[parentIndices[i]];
        float scaledRadius = worldRadius * (0.5f + elite.genome.size * 1.5f);
        float x = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        float z = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        nextGen.emplace_back(elite.genome, elite.phenotype,
                             glm::vec3(x, 3.0f, z), 1.0f, scaledRadius);
    }

    // Breed remainder
    while ((int)nextGen.size() < targetCount) {
        Genome    child = Mutate(Crossover(blobs[selectParent()].genome,
                                           blobs[selectParent()].genome),
                                 0.3f, 0.15f);
        Phenotype pheno = MakePhenotype(child, baseScale, worldRadius);
        float scaledRadius = worldRadius * (0.5f + child.size * 1.5f);
        float x = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        float z = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        nextGen.emplace_back(child, pheno, glm::vec3(x, 3.0f, z), 1.0f, scaledRadius);
    }

    blobs = std::move(nextGen);
    gGeneration.timer   = 0.0f;
    gGeneration.running = true;

    std::cout << "  Spawned " << blobs.size() << " blobs for gen "
              << gGeneration.number << std::endl;
}

// --------------------- Logging ---------------------

void LogGeneration(const std::vector<BlobInstance>& blobs,
                   const std::vector<std::pair<float, int>>& scored,
                   int generationNumber) {

    GenerationRecord rec;
    rec.number         = generationNumber;
    rec.totalBlobs     = (int)blobs.size();
    rec.survivors      = 0;
    rec.totalKills     = 0;
    rec.totalFoodEaten = 0;

    float totalFitness      = 0.0f;
    float totalSize         = 0.0f;
    float totalSpeed        = 0.0f;
    float totalPredation    = 0.0f;
    float totalAggression   = 0.0f;
    float totalTimeAlive    = 0.0f;
    float totalEnergyGained = 0.0f;
    float bestFitness       = -1.0f;

    for (auto& [fitness, idx] : scored) {
        const BlobInstance& b = blobs[idx];

        IndividualRecord ir;
        ir.generationNumber = generationNumber;
        ir.blobIndex        = idx;
        ir.survived         = b.alive;
        ir.fitness          = fitness;
        ir.timeAlive        = b.stats.timeAlive;
        ir.energyGained     = b.stats.energyGained;
        ir.healthLost       = b.stats.healthLost;
        ir.damageDealt      = b.stats.damageDealt;
        ir.foodEaten        = b.stats.foodEaten;
        ir.killCount        = b.stats.killCount;
        ir.gSize            = b.genome.size;
        ir.gSpeed           = b.genome.speed;
        ir.gPredation       = b.genome.predation;
        ir.gAggression      = b.genome.aggression;
        ir.worldScale       = b.phenotype.worldScale;
        ir.moveForce        = b.phenotype.moveForce;
        ir.maxEnergy        = b.phenotype.maxEnergy;
        ir.energyCost       = b.phenotype.energyCost;
        ir.attackDamage     = b.phenotype.attackDamage;
        ir.maxHealth        = b.phenotype.maxHealth;

        rec.individuals.push_back(ir);

        if (b.alive) rec.survivors++;
        totalFitness      += fitness;
        totalSize         += b.genome.size;
        totalSpeed        += b.genome.speed;
        totalPredation    += b.genome.predation;
        totalAggression   += b.genome.aggression;
        totalTimeAlive    += b.stats.timeAlive;
        totalEnergyGained += b.stats.energyGained;
        rec.totalKills     += b.stats.killCount;
        rec.totalFoodEaten += b.stats.foodEaten;

        if (fitness > bestFitness) {
            bestFitness        = fitness;
            rec.bestIndividual = ir;
        }
    }

    int aliveCount = 0;
    for (auto& b : blobs)
        if (b.alive) aliveCount++;
    int n = std::max(1, aliveCount);
    rec.bestFitness     = bestFitness;
    rec.avgFitness      = totalFitness      / n;
    rec.avgSize         = totalSize         / n;
    rec.avgSpeed        = totalSpeed        / n;
    rec.avgPredation    = totalPredation    / n;
    rec.avgAggression   = totalAggression   / n;
    rec.avgTimeAlive    = totalTimeAlive    / n;
    rec.avgEnergyGained = totalEnergyGained / n;

    gGenerationLog.push_back(rec);
}

void ExportGenerationCSV(const GenerationRecord& rec, const std::string& folder) {
    std::filesystem::create_directories(folder);

    std::string filename = folder + "/generation_" + std::to_string(rec.number) + ".csv";
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open CSV file for writing: " << filename << std::endl;
        return;
    }

    file << "generation,blobIndex,survived,fitness,timeAlive,energyGained,healthLost,"
            "damageDealt,foodEaten,killCount,gSize,gSpeed,gPredation,gAggression,"
            "worldScale,moveForce,maxEnergy,energyCost,attackDamage,maxHealth\n";

    for (const auto& ir : rec.individuals) {
        file << ir.generationNumber << ","
             << ir.blobIndex << ","
             << (ir.survived ? 1 : 0) << ","
             << ir.fitness << ","
             << ir.timeAlive << ","
             << ir.energyGained << ","
             << ir.healthLost << ","
             << ir.damageDealt << ","
             << ir.foodEaten << ","
             << ir.killCount << ","
             << ir.gSize << ","
             << ir.gSpeed << ","
             << ir.gPredation << ","
             << ir.gAggression << ","
             << ir.worldScale << ","
             << ir.moveForce << ","
             << ir.maxEnergy << ","
             << ir.energyCost << ","
             << ir.attackDamage << ","
             << ir.maxHealth << "\n";
    }
}

void ExportGenerationSummaryCSV(const std::vector<GenerationRecord>& log,
                                const std::string& filename) {
    std::filesystem::create_directories("gen_logs");

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open summary CSV file for writing: " << filename << std::endl;
        return;
    }

    file << "generation,totalBlobs,survivors,totalKills,totalFoodEaten,bestFitness,"
            "avgFitness,avgSize,avgSpeed,avgPredation,avgAggression,avgTimeAlive,avgEnergyGained\n";

    for (const auto& rec : log) {
        file << rec.number << ","
             << rec.totalBlobs << ","
             << rec.survivors << ","
             << rec.totalKills << ","
             << rec.totalFoodEaten << ","
             << rec.bestFitness << ","
             << rec.avgFitness << ","
             << rec.avgSize << ","
             << rec.avgSpeed << ","
             << rec.avgPredation << ","
             << rec.avgAggression << ","
             << rec.avgTimeAlive << ","
             << rec.avgEnergyGained << "\n";
    }
}