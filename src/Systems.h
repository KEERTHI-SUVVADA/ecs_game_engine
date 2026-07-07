#pragma once
#include "World.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <string>

// ============================================================
// SYSTEMS — all game logic. Each loops over entities and
// processes only those with the required components.
// ============================================================


// ------------------------------------------------------------
// InputSystem
//   Reads keyboard → sets player Velocity.
//   Also tracks facing direction for bullet spawning.
// ------------------------------------------------------------
class InputSystem {
public:
    void update(World& world) {
        for (Entity e = 0; e < world.entityCount(); e++) {
            Input*    inp = world.getInput(e);
            Velocity* vel = world.getVelocity(e);
            if (!inp || !vel) continue;

            vel->dx = 0.0f;
            vel->dy = 0.0f;
            float s = inp->speed;

            bool moved = false;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)  || sf::Keyboard::isKeyPressed(sf::Keyboard::A)) { vel->dx = -s; inp->facingX = -1; inp->facingY =  0; moved = true; }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::D)) { vel->dx =  s; inp->facingX =  1; inp->facingY =  0; moved = true; }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)    || sf::Keyboard::isKeyPressed(sf::Keyboard::W)) { vel->dy = -s; inp->facingX =  0; inp->facingY = -1; moved = true; }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)  || sf::Keyboard::isKeyPressed(sf::Keyboard::S)) { vel->dy =  s; inp->facingX =  0; inp->facingY =  1; moved = true; }
            (void)moved;
        }
    }
};


// ------------------------------------------------------------
// BulletSpawnSystem
//   Called when Space is pressed (checked in main.cpp via event).
//   Spawns a bullet at player position flying in facing direction.
// ------------------------------------------------------------
class BulletSpawnSystem {
public:
    void spawnBullet(World& world, float bulletSpeed = 500.0f) {
        // Find player
        for (Entity e = 0; e < world.entityCount(); e++) {
            Input*    inp = world.getInput(e);
            Position* pos = world.getPosition(e);
            if (!inp || !pos) continue;

            Entity b = world.createEntity();
            world.addTag(b,        {Tag::BULLET});
            world.addPosition(b,   {pos->x, pos->y});                         // start at player centre
            world.addVelocity(b,   {inp->facingX * bulletSpeed,
                                    inp->facingY * bulletSpeed});              // fly in facing direction
            world.addRenderable(b, {5.0f, 1.0f, 0.95f, 0.3f});               // small yellow-white circle
            world.addBullet(b);
            break;
        }
    }
};


// ------------------------------------------------------------
// BulletSystem
//   Destroys bullets that leave the screen.
//   (Bullet-vs-enemy hits are handled by CollisionSystem.)
// ------------------------------------------------------------
class BulletSystem {
public:
    void update(World& world, float W, float H) {
        for (Entity e = 0; e < world.entityCount(); e++) {
            if (!world.getBullet(e)) continue;
            if (world.isDead(e)) continue;

            Position* pos = world.getPosition(e);
            if (!pos) continue;

            // Off screen? Mark dead.
            if (pos->x < 0 || pos->x > W || pos->y < 0 || pos->y > H)
                world.markDead(e);
        }
    }
};


// ------------------------------------------------------------
// ChaseSystem
//   Needs: Chase + Position + Velocity (and Tag == ENEMY)
//   Does:  each frame, points enemy velocity toward the player.
//
//   How direction-toward-player works:
//     1. Compute vector from enemy to player: (dx, dy)
//     2. Normalise it to length 1:  divide by sqrt(dx²+dy²)
//     3. Multiply by chase speed
//   Result: enemy always moves at constant speed toward player.
// ------------------------------------------------------------
class ChaseSystem {
public:
    void update(World& world) {
        // Find player position first
        Position* playerPos = nullptr;
        for (Entity e = 0; e < world.entityCount(); e++) {
            TagComponent* t = world.getTag(e);
            if (t && t->tag == Tag::PLAYER) {
                playerPos = world.getPosition(e);
                break;
            }
        }
        if (!playerPos) return;  // no player, nothing to chase

        // Steer every enemy toward the player
        for (Entity e = 0; e < world.entityCount(); e++) {
            if (world.isDead(e)) continue;

            Chase*    chase = world.getChase(e);
            Position* pos   = world.getPosition(e);
            Velocity* vel   = world.getVelocity(e);
            if (!chase || !pos || !vel) continue;

            float dx   = playerPos->x - pos->x;
            float dy   = playerPos->y - pos->y;
            float dist = std::sqrt(dx*dx + dy*dy);

            if (dist < 1.0f) continue;  // already on top of player, don't divide by zero

            // Normalise then scale by speed
            vel->dx = (dx / dist) * chase->speed;
            vel->dy = (dy / dist) * chase->speed;
        }
    }
};


// ------------------------------------------------------------
// MovementSystem — moves all entities, bounces off walls.
// Bullets are NOT bounced — they fly off screen and get deleted.
// ------------------------------------------------------------
class MovementSystem {
public:
    void update(World& world, float dt, float W, float H) {
        for (Entity e = 0; e < world.entityCount(); e++) {
            if (world.isDead(e)) continue;

            Position*    pos = world.getPosition(e);
            Velocity*    vel = world.getVelocity(e);
            Renderable*  ren = world.getRenderable(e);
            TagComponent* t  = world.getTag(e);
            if (!pos || !vel) continue;

            pos->x += vel->dx * dt;
            pos->y += vel->dy * dt;

            // Bullets fly through — no bouncing
            bool isBullet = (t && t->tag == Tag::BULLET);
            if (isBullet) continue;

            float r = ren ? ren->radius : 5.0f;
            if (pos->x - r < 0)   { pos->x = r;     vel->dx =  std::abs(vel->dx); }
            if (pos->x + r > W)   { pos->x = W - r; vel->dx = -std::abs(vel->dx); }
            if (pos->y - r < 0)   { pos->y = r;     vel->dy =  std::abs(vel->dy); }
            if (pos->y + r > H)   { pos->y = H - r; vel->dy = -std::abs(vel->dy); }
        }
    }
};


// ------------------------------------------------------------
// CollisionSystem
//   Checks two types of collision:
//     1. Bullet  vs Enemy  → enemy dies, bullet dies
//     2. Player  vs Enemy  → enemy dies, player loses health
//
//   Uses distance² to avoid sqrt() (same result, faster).
// ------------------------------------------------------------
class CollisionSystem {
public:
    int update(World& world) {
        int killed = 0;

        // Gather player info
        Entity    playerEntity = INVALID_ENTITY;
        Position* playerPos    = nullptr;
        float     playerRadius = 18.0f;
        Health*   playerHealth = nullptr;

        for (Entity e = 0; e < world.entityCount(); e++) {
            TagComponent* t = world.getTag(e);
            if (t && t->tag == Tag::PLAYER) {
                playerEntity = e;
                playerPos    = world.getPosition(e);
                playerHealth = world.getHealth(e);
                Renderable* r = world.getRenderable(e);
                if (r) playerRadius = r->radius;
                break;
            }
        }

        // Check every enemy
        for (Entity enemy = 0; enemy < world.entityCount(); enemy++) {
            TagComponent* et = world.getTag(enemy);
            if (!et || et->tag != Tag::ENEMY) continue;
            if (world.isDead(enemy)) continue;

            Position*   ePos = world.getPosition(enemy);
            Renderable* eRen = world.getRenderable(enemy);
            if (!ePos) continue;
            float er = eRen ? eRen->radius : 14.0f;

            // --- Bullet vs Enemy ---
            for (Entity bullet = 0; bullet < world.entityCount(); bullet++) {
                if (!world.getBullet(bullet)) continue;
                if (world.isDead(bullet)) continue;

                Position* bPos = world.getPosition(bullet);
                if (!bPos) continue;

                float bdx = bPos->x - ePos->x;
                float bdy = bPos->y - ePos->y;
                float minD = 5.0f + er;  // bullet radius (5) + enemy radius

                if (bdx*bdx + bdy*bdy < minD*minD) {
                    world.markDead(enemy);
                    world.markDead(bullet);
                    killed++;
                    break;  // this enemy is dead, stop checking bullets for it
                }
            }

            if (world.isDead(enemy)) continue;

            // --- Player vs Enemy ---
            if (!playerPos) continue;
            float pdx  = playerPos->x - ePos->x;
            float pdy  = playerPos->y - ePos->y;
            float minD = playerRadius + er;

            if (pdx*pdx + pdy*pdy < minD*minD) {
                world.markDead(enemy);
                killed++;
                // Player takes damage on contact
                if (playerHealth) {
                    playerHealth->current -= 20.0f;
                    if (playerHealth->current < 0) playerHealth->current = 0;
                }
            }
        }

        return killed;
    }
};


// ------------------------------------------------------------
// RenderSystem — draws all visible entities.
//   Player  → yellow with white outline
//   Enemy   → red, slightly larger
//   Bullet  → small bright dot with glow outline
//   Others  → plain circle
// ------------------------------------------------------------
class RenderSystem {
public:
    void render(World& world, sf::RenderWindow& window) {
        for (Entity e = 0; e < world.entityCount(); e++) {
            if (world.isDead(e)) continue;

            Position*    pos = world.getPosition(e);
            Renderable*  ren = world.getRenderable(e);
            TagComponent* t  = world.getTag(e);
            if (!pos || !ren) continue;

            sf::CircleShape shape(ren->radius);
            shape.setOrigin(ren->radius, ren->radius);
            shape.setPosition(pos->x, pos->y);
            shape.setFillColor(sf::Color(
                static_cast<sf::Uint8>(ren->r * 255),
                static_cast<sf::Uint8>(ren->g * 255),
                static_cast<sf::Uint8>(ren->b * 255)
            ));

            if (t) {
                if (t->tag == Tag::PLAYER) {
                    shape.setOutlineThickness(3.0f);
                    shape.setOutlineColor(sf::Color::White);
                } else if (t->tag == Tag::BULLET) {
                    shape.setOutlineThickness(2.0f);
                    shape.setOutlineColor(sf::Color(255, 240, 100, 180));
                }
            }

            window.draw(shape);
        }
    }
};


// ------------------------------------------------------------
// HealthSystem — draws health bars
// ------------------------------------------------------------
class HealthSystem {
public:
    void render(World& world, sf::RenderWindow& window) {
        for (Entity e = 0; e < world.entityCount(); e++) {
            if (world.isDead(e)) continue;
            Position* pos    = world.getPosition(e);
            Health*   health = world.getHealth(e);
            if (!pos || !health) continue;

            float barW  = 30.0f;
            float barH  = 4.0f;
            float fillW = barW * (health->current / health->max);
            float bx    = pos->x - barW / 2.0f;
            float by    = pos->y - 22.0f;

            sf::RectangleShape bg({barW, barH});
            bg.setPosition(bx, by);
            bg.setFillColor(sf::Color(180, 40, 40));
            window.draw(bg);

            sf::RectangleShape fg({fillW, barH});
            fg.setPosition(bx, by);
            fg.setFillColor(sf::Color(60, 200, 80));
            window.draw(fg);
        }
    }
};


// ------------------------------------------------------------
// HUDSystem — score, enemy count, controls hint
// ------------------------------------------------------------
class HUDSystem {
public:
    bool init(const std::string& fontPath) {
        return m_font.loadFromFile(fontPath);
    }

    void render(sf::RenderWindow& window, int score, int enemyCount, int bulletCount) {
        auto draw = [&](const std::string& str, float x, float y, unsigned sz, sf::Color col) {
            sf::Text t;
            t.setFont(m_font);
            t.setString(str);
            t.setCharacterSize(sz);
            t.setFillColor(col);
            t.setPosition(x, y);
            window.draw(t);
        };

        draw("Score: "   + std::to_string(score),       20, 16, 24, sf::Color::White);
        draw("Enemies: " + std::to_string(enemyCount),  20, 46, 18, sf::Color(255,120,120));
        draw("Bullets: " + std::to_string(bulletCount), 20, 70, 18, sf::Color(255,240,100));
        draw("WASD/Arrows: move   |   Space: shoot   |   Esc: quit",
             20, window.getSize().y - 28.0f, 14, sf::Color(150,150,150));
    }

private:
    sf::Font m_font;
};