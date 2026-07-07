#include <SFML/Graphics.hpp>
#include <cstdlib>
#include <ctime>
#include "imgui.h"
#include "imgui-SFML.h"
#include "World.h"
#include "Systems.h"
#include "Inspector.h"

float randFloat(float lo, float hi) {
    return lo + (hi - lo) * (std::rand() / (float)RAND_MAX);
}

void spawnEnemy(World& world, float W, float H) {
    Entity e = world.createEntity();
    float x, y;
    int edge = std::rand() % 4;
    if      (edge == 0) { x = randFloat(0,W); y = 0;  }
    else if (edge == 1) { x = randFloat(0,W); y = H;  }
    else if (edge == 2) { x = 0;  y = randFloat(0,H); }
    else                { x = W;  y = randFloat(0,H); }

    world.addTag(e,        {Tag::ENEMY});
    world.addPosition(e,   {x, y});
    world.addVelocity(e,   {0.0f, 0.0f});
    world.addRenderable(e, {14.0f, 0.9f, 0.25f, 0.25f});
    world.addHealth(e,     {100.0f, 100.0f});
    world.addChase(e,      {45.0f});   // slow chase speed
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const float W = 1200.0f, H = 700.0f;
    sf::RenderWindow window(sf::VideoMode((unsigned)W, (unsigned)H),
                            "ECS Demo  |  WASD: move  Space: shoot  F1: toggle inspector");
    window.setFramerateLimit(60);

    // Initialise ImGui-SFML — must happen after window is created
    (void)ImGui::SFML::Init(window);

    // --- SYSTEMS ---
    InputSystem       inputSys;
    BulletSpawnSystem bulletSpawnSys;
    BulletSystem      bulletSys;
    ChaseSystem       chaseSys;
    MovementSystem    moveSys;
    CollisionSystem   collisionSys;
    RenderSystem      renderSys;
    HealthSystem      healthSys;
    HUDSystem         hudSys;
    ComponentInspector inspector;

    if (!hudSys.init("C:/Windows/Fonts/arial.ttf"))
        hudSys.init("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

    World world;

    // --- PLAYER ---
    Entity player = world.createEntity();
    world.addTag(player,        {Tag::PLAYER});
    world.addPosition(player,   {W / 2.0f, H / 2.0f});
    world.addVelocity(player,   {0.0f, 0.0f});
    world.addRenderable(player, {18.0f, 1.0f, 0.85f, 0.1f});
    world.addHealth(player,     {100.0f, 100.0f});
    world.addInput(player,      {220.0f, 1.0f, 0.0f});

   // --- DECORATIVE BALLS ---
for (int i = 0; i < 30; i++) {
    Entity e = world.createEntity();
    world.addTag(e,        {Tag::DECORATION});   // ← add this line
    world.addPosition(e,   {randFloat(20,W-20), randFloat(20,H-20)});
    world.addVelocity(e,   {randFloat(-70,70),  randFloat(-70,70)});
    world.addRenderable(e, {randFloat(4,9), randFloat(0.2f,0.5f), randFloat(0.2f,0.5f), randFloat(0.2f,0.5f)});
}

    // --- INITIAL ENEMIES ---
    for (int i = 0; i < 6; i++) spawnEnemy(world, W, H);

    // --- TREES ---
    for (int i = 0; i < 8; i++) {
        Entity e = world.createEntity();
        world.addTag(e,        {Tag::DECORATION});
        world.addPosition(e,   {randFloat(40,W-40), randFloat(40,H-40)});
        world.addRenderable(e, {16.0f, 0.15f, 0.65f, 0.2f});
    }

    // --- STATE ---
    int   score         = 0;
    float spawnTimer    = 0.0f;
    float spawnEvery    = 3.0f;
    float shootCooldown = 0.0f;
    bool  showInspector = true;
    bool  gameOver      = false;

    sf::Clock deltaClock;   // ImGui-SFML needs its own clock

    while (window.isOpen()) {

        // --- EVENTS ---
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);  // let ImGui see events first

            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();
                if (event.key.code == sf::Keyboard::F1) showInspector = !showInspector;
                if (!gameOver && event.key.code == sf::Keyboard::Space && shootCooldown <= 0.0f) {
                    bulletSpawnSys.spawnBullet(world);
                    shootCooldown = 0.2f;
                }
                // R restarts — simplest approach is just close and reopen
                if (gameOver && event.key.code == sf::Keyboard::R) {
                    window.close();
                }
            }
        }

        sf::Time dt_time = deltaClock.restart();
        float dt = dt_time.asSeconds();
        if (dt > 0.05f) dt = 0.05f;
        shootCooldown -= dt;

        // Update ImGui frame
        ImGui::SFML::Update(window, dt_time);

        int enemyCount = 0, bulletCount = 0;
        if (!gameOver) {
            // --- SPAWNER ---
            spawnTimer += dt;
            if (spawnTimer >= spawnEvery) {
                spawnEnemy(world, W, H);
                spawnTimer = 0.0f;
            }

            // --- GAME SYSTEMS ---
            inputSys.update(world);
            chaseSys.update(world);
            moveSys.update(world, dt, W, H);
            bulletSys.update(world, W, H);
            score += collisionSys.update(world);
            world.cleanup();

            // --- COUNT FOR HUD ---
            for (Entity e = 0; e < world.entityCount(); e++) {
                if (world.isDead(e)) continue;
                TagComponent* t = world.getTag(e);
                if (!t) continue;
                if (t->tag == Tag::ENEMY)  enemyCount++;
                if (t->tag == Tag::BULLET) bulletCount++;
            }
        }

        // --- IMGUI PANELS ---
        if (showInspector)
            inspector.render(world, score);

        // --- CHECK PLAYER HEALTH ---
        Health* playerHealth = world.getHealth(player);
        if (playerHealth && playerHealth->current <= 0 && !gameOver) {
            gameOver = true;
        }

        // --- DRAW ---
        window.clear(sf::Color(18, 18, 22));

        if (gameOver) {
            // Load font for game over screen (same font as HUD)
            static sf::Font font;
            static bool fontLoaded = false;
            if (!fontLoaded) {
                if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf"))
                    font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
                fontLoaded = true;
            }

            // "YOU DIED" big red text
            sf::Text title;
            title.setFont(font);
            title.setString("YOU DIED");
            title.setCharacterSize(80);
            title.setFillColor(sf::Color(200, 30, 30));
            title.setStyle(sf::Text::Bold);
            sf::FloatRect tb = title.getLocalBounds();
            title.setOrigin(tb.width / 2, tb.height / 2);
            title.setPosition(W / 2.0f, H / 2.0f - 60.0f);
            window.draw(title);

            // Final score
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Final Score: " + std::to_string(score));
            scoreText.setCharacterSize(32);
            scoreText.setFillColor(sf::Color::White);
            sf::FloatRect sb = scoreText.getLocalBounds();
            scoreText.setOrigin(sb.width / 2, sb.height / 2);
            scoreText.setPosition(W / 2.0f, H / 2.0f + 20.0f);
            window.draw(scoreText);

            // Restart hint
            sf::Text hint;
            hint.setFont(font);
            hint.setString("Press R to restart   |   Esc to quit");
            hint.setCharacterSize(20);
            hint.setFillColor(sf::Color(160, 160, 160));
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin(hb.width / 2, hb.height / 2);
            hint.setPosition(W / 2.0f, H / 2.0f + 80.0f);
            window.draw(hint);

        } else {
            renderSys.render(world, window);
            healthSys.render(world, window);
            hudSys.render(window, score, enemyCount, bulletCount);
            if (showInspector) inspector.render(world, score);
            ImGui::SFML::Render(window);
        }

        window.display();
    }

    ImGui::SFML::Shutdown();
}