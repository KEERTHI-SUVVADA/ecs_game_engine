#pragma once
#include "Components.h"
#include <vector>
#include <optional>
#include <cstring>

// ============================================================
// WHY WE CHANGED FROM unordered_map TO vector
// ============================================================
//
// The old way stored components like this:
//
//   unordered_map<Entity, Position>  positions;
//
// A hash map stores each value at a RANDOM memory location.
// When a system loops 1000 entities, the CPU has to jump to
// 1000 different places in RAM to fetch each Position.
// Every jump risks a "cache miss" — the CPU stalls waiting
// for RAM (hundreds of nanoseconds each time).
//
// The new way stores components like this:
//
//   Position positionData[MAX_ENTITIES];  // one flat array
//   bool     hasPosition[MAX_ENTITIES];   // does entity[i] have one?
//
// All Positions sit side-by-side in memory. When the CPU
// fetches positionData[0], hardware prefetching pulls in
// positionData[1..15] for free (a "cache line" = 64 bytes).
// The next 15 lookups are instant — no stalls.
//
// This is called Data-Oriented Design (DOD).
// It's the core reason ECS exists in engines like Frostbite,
// Unity DOTS, and Unreal's Mass framework.
//
// In an interview, say:
//   "I changed component storage from hash maps to flat arrays
//    so that systems iterate contiguous memory. This improves
//    cache locality — the CPU prefetcher can stay ahead of the
//    loop, eliminating cache misses during hot system updates."
//
// ============================================================

class World {
public:

    World() {
        // Fill the sparse index with INVALID so we know which
        // slots are empty (INVALID = entity has no component)
        for (auto& arr : {
            (unsigned*)entityToTag,   (unsigned*)entityToPos,
            (unsigned*)entityToVel,   (unsigned*)entityToRen,
            (unsigned*)entityToHealth,(unsigned*)entityToInput,
            (unsigned*)entityToChase, (unsigned*)entityToBullet,
            (unsigned*)entityToDead
        }) {
            for (int i = 0; i < MAX_ENTITIES; i++) arr[i] = INVALID_ENTITY;
        }
    }

    // -------------------------------------------------------
    // CREATE / DESTROY
    // -------------------------------------------------------

    Entity createEntity() {
        // Reuse a freed slot if one exists, otherwise take next
        if (!m_freeList.empty()) {
            Entity e = m_freeList.back();
            m_freeList.pop_back();
            return e;
        }
        return m_nextId++;
    }

    void markDead(Entity e) {
        if (e >= MAX_ENTITIES) return;
        deadData[e] = Dead{};
        entityToDead[e] = e;
    }

    void cleanup() {
        for (unsigned e = 0; e < m_nextId; e++) {
            if (entityToDead[e] == INVALID_ENTITY) continue;
            // Remove from every component array
            entityToTag[e]    = INVALID_ENTITY;
            entityToPos[e]    = INVALID_ENTITY;
            entityToVel[e]    = INVALID_ENTITY;
            entityToRen[e]    = INVALID_ENTITY;
            entityToHealth[e] = INVALID_ENTITY;
            entityToInput[e]  = INVALID_ENTITY;
            entityToChase[e]  = INVALID_ENTITY;
            entityToBullet[e] = INVALID_ENTITY;
            entityToDead[e]   = INVALID_ENTITY;
            m_freeList.push_back(e);  // slot is now reusable
        }
    }

    // -------------------------------------------------------
    // ADD COMPONENTS
    //
    // We store the component data directly at index [entity]
    // in a flat array. entityToXxx[e] acts as a presence flag:
    //   == INVALID_ENTITY  →  entity does NOT have this component
    //   == e               →  entity DOES have it, data at [e]
    // -------------------------------------------------------

    void addTag(Entity e, TagComponent t)      { tagData[e]    = t; entityToTag[e]    = e; }
    void addPosition(Entity e, Position p)     { posData[e]    = p; entityToPos[e]    = e; }
    void addVelocity(Entity e, Velocity v)     { velData[e]    = v; entityToVel[e]    = e; }
    void addRenderable(Entity e, Renderable r) { renData[e]    = r; entityToRen[e]    = e; }
    void addHealth(Entity e, Health h)         { healthData[e] = h; entityToHealth[e] = e; }
    void addInput(Entity e, Input i)           { inputData[e]  = i; entityToInput[e]  = e; }
    void addChase(Entity e, Chase c)           { chaseData[e]  = c; entityToChase[e]  = e; }
    void addBullet(Entity e)                   { bulletData[e] = Bullet{}; entityToBullet[e] = e; }

    // -------------------------------------------------------
    // GET COMPONENTS
    //
    // Returns a pointer directly into the flat array.
    // Pointer is valid until cleanup() runs — don't cache across frames.
    // Returns nullptr if entity doesn't have the component.
    // -------------------------------------------------------

    TagComponent* getTag(Entity e)      { return (e<MAX_ENTITIES && entityToTag[e]   !=INVALID_ENTITY) ? &tagData[e]    : nullptr; }
    Position*     getPosition(Entity e) { return (e<MAX_ENTITIES && entityToPos[e]   !=INVALID_ENTITY) ? &posData[e]    : nullptr; }
    Velocity*     getVelocity(Entity e) { return (e<MAX_ENTITIES && entityToVel[e]   !=INVALID_ENTITY) ? &velData[e]    : nullptr; }
    Renderable*   getRenderable(Entity e){return (e<MAX_ENTITIES && entityToRen[e]   !=INVALID_ENTITY) ? &renData[e]    : nullptr; }
    Health*       getHealth(Entity e)   { return (e<MAX_ENTITIES && entityToHealth[e]!=INVALID_ENTITY) ? &healthData[e] : nullptr; }
    Input*        getInput(Entity e)    { return (e<MAX_ENTITIES && entityToInput[e] !=INVALID_ENTITY) ? &inputData[e]  : nullptr; }
    Chase*        getChase(Entity e)    { return (e<MAX_ENTITIES && entityToChase[e] !=INVALID_ENTITY) ? &chaseData[e]  : nullptr; }
    Bullet*       getBullet(Entity e)   { return (e<MAX_ENTITIES && entityToBullet[e]!=INVALID_ENTITY) ? &bulletData[e] : nullptr; }
    bool          isDead(Entity e)      { return  e<MAX_ENTITIES && entityToDead[e]  !=INVALID_ENTITY; }

    unsigned int entityCount() const { return m_nextId; }

private:
    unsigned int           m_nextId = 0;
    std::vector<Entity>    m_freeList;   // recycled entity IDs

    // -------------------------------------------------------
    // FLAT COMPONENT ARRAYS — the core of DOD.
    //
    // posData[5]  = Position data for entity 5
    // entityToPos[5] = 5 means "entity 5 has a Position"
    //                = INVALID_ENTITY means "it doesn't"
    //
    // All data for one component type is contiguous in RAM.
    // Systems that loop posData[] stay in CPU cache the entire time.
    // -------------------------------------------------------

    TagComponent tagData   [MAX_ENTITIES];  unsigned entityToTag   [MAX_ENTITIES];
    Position     posData   [MAX_ENTITIES];  unsigned entityToPos   [MAX_ENTITIES];
    Velocity     velData   [MAX_ENTITIES];  unsigned entityToVel   [MAX_ENTITIES];
    Renderable   renData   [MAX_ENTITIES];  unsigned entityToRen   [MAX_ENTITIES];
    Health       healthData[MAX_ENTITIES];  unsigned entityToHealth[MAX_ENTITIES];
    Input        inputData [MAX_ENTITIES];  unsigned entityToInput [MAX_ENTITIES];
    Chase        chaseData [MAX_ENTITIES];  unsigned entityToChase [MAX_ENTITIES];
    Bullet       bulletData[MAX_ENTITIES];  unsigned entityToBullet[MAX_ENTITIES];
    Dead         deadData  [MAX_ENTITIES];  unsigned entityToDead  [MAX_ENTITIES];
};