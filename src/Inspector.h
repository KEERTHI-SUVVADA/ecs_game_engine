#pragma once
#include "World.h"
#include "imgui.h"
#include <string>

// ============================================================
// ComponentInspector
//
// Draws an ImGui debug panel showing every live entity,
// its tag, and the current values of all its components.
//
// This is the kind of tool you'd find in real game engines
// (Unreal's Details Panel, Unity's Inspector, etc.).
// Having it in a portfolio project signals you understand
// that game dev is not just gameplay — it's tooling too.
// ============================================================

class ComponentInspector {
public:

    void render(World& world, int score) {

        // --- Left panel: entity list ---
        ImGui::SetNextWindowPos ({10,  10},  ImGuiCond_Once);
        ImGui::SetNextWindowSize({260, 580}, ImGuiCond_Once);
        ImGui::Begin("Entity List", nullptr, ImGuiWindowFlags_NoMove);

        ImGui::Text("Score: %d", score);
        ImGui::Separator();

        int total = 0, enemies = 0, bullets = 0;

        for (Entity e = 0; e < world.entityCount(); e++) {
            if (world.isDead(e)) continue;
            TagComponent* t = world.getTag(e);
            total++;
            if (t && t->tag == Tag::ENEMY)  enemies++;
            if (t && t->tag == Tag::BULLET) bullets++;

            // Build a label like "[#3] ENEMY"
            std::string label = "[#" + std::to_string(e) + "] ";
            if (!t) label += "no tag";
            else switch (t->tag) {
                case Tag::PLAYER:     label += "PLAYER";     break;
                case Tag::ENEMY:      label += "ENEMY";      break;
                case Tag::BULLET:     label += "BULLET";     break;
                case Tag::DECORATION: label += "DECORATION"; break;
            }

            // Clicking an entity selects it for the detail panel
            bool selected = (m_selected == (int)e);
            if (ImGui::Selectable(label.c_str(), selected))
                m_selected = (int)e;
        }

        ImGui::Separator();
        ImGui::Text("Total:   %d", total);
        ImGui::Text("Enemies: %d", enemies);
        ImGui::Text("Bullets: %d", bullets);
        ImGui::End();

        // --- Right panel: component detail for selected entity ---
        ImGui::SetNextWindowPos ({280, 10},  ImGuiCond_Once);
        ImGui::SetNextWindowSize({300, 400}, ImGuiCond_Once);
        ImGui::Begin("Component Inspector", nullptr, ImGuiWindowFlags_NoMove);

        if (m_selected < 0 || (unsigned)m_selected >= world.entityCount()
            || world.isDead((Entity)m_selected)) {
            ImGui::Text("Click an entity on the left.");
            ImGui::End();
            return;
        }

        Entity e = (Entity)m_selected;
        ImGui::Text("Entity #%u", e);
        ImGui::Separator();

        // Show each component only if the entity has it
        if (auto* p = world.getPosition(e)) {
            if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  x: %.1f", p->x);
                ImGui::Text("  y: %.1f", p->y);
            }
        }

        if (auto* v = world.getVelocity(e)) {
            if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  dx: %.1f", v->dx);
                ImGui::Text("  dy: %.1f", v->dy);
                float speed = std::sqrt(v->dx*v->dx + v->dy*v->dy);
                ImGui::Text("  |v|: %.1f", speed);  // magnitude
            }
        }

        if (auto* r = world.getRenderable(e)) {
            if (ImGui::CollapsingHeader("Renderable", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  radius: %.1f", r->radius);
                // Show a colour swatch next to the rgb values
                ImVec4 col(r->r, r->g, r->b, 1.0f);
                ImGui::ColorButton("##col", col, 0, ImVec2(14,14));
                ImGui::SameLine();
                ImGui::Text("rgb(%.2f, %.2f, %.2f)", r->r, r->g, r->b);
            }
        }

        if (auto* h = world.getHealth(e)) {
            if (ImGui::CollapsingHeader("Health", ImGuiTreeNodeFlags_DefaultOpen)) {
                float pct = h->current / h->max;
                ImGui::Text("  %.0f / %.0f", h->current, h->max);
                // Progress bar — green to red based on health
                ImVec4 barCol = pct > 0.5f
                    ? ImVec4(0.2f, 0.8f, 0.3f, 1.f)
                    : ImVec4(0.9f, 0.2f, 0.2f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barCol);
                ImGui::ProgressBar(pct, ImVec2(-1, 0));
                ImGui::PopStyleColor();
            }
        }

        if (auto* inp = world.getInput(e)) {
            if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  speed:   %.1f", inp->speed);
                ImGui::Text("  facingX: %.1f", inp->facingX);
                ImGui::Text("  facingY: %.1f", inp->facingY);
            }
        }

        if (auto* c = world.getChase(e)) {
            if (ImGui::CollapsingHeader("Chase", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  speed: %.1f", c->speed);
            }
        }

        if (world.getBullet(e)) {
            if (ImGui::CollapsingHeader("Bullet", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("  (marker — no data)");
            }
        }

        ImGui::End();

    }

private:
    int m_selected = -1;
};
