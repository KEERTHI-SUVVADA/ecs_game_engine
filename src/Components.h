#pragma once
#include <cmath>

using Entity = unsigned int;
const Entity INVALID_ENTITY  = 999999;
const unsigned int MAX_ENTITIES = 1024;  // maximum live entities at once

// ============================================================
// TAG
// ============================================================
enum class Tag { PLAYER, ENEMY, BULLET, DECORATION };

struct TagComponent { Tag tag = Tag::DECORATION; };

// ============================================================
// COMPONENTS — plain data, no logic
// ============================================================

struct Position   { float x = 0, y = 0; };
struct Velocity   { float dx = 0, dy = 0; };
struct Renderable { float radius = 10, r = 1, g = 1, b = 1; };
struct Health     { float current = 100, max = 100; };
struct Input      { float speed = 220, facingX = 1, facingY = 0; };
struct Chase      { float speed = 45.0f; };   // <-- reduced from 90
struct Bullet     {};
struct Dead       {};