#include "pch.h"
#include "runtime_hud.h"

#include <imgui.h>

namespace NexAur {
    namespace {
        const char* getStateText(GameState state) {
            switch (state) {
            case GameState::MainMenu:
                return "MAIN MENU";
            case GameState::Playing:
                return "PLAYING";
            case GameState::Paused:
                return "PAUSED";
            case GameState::GameOver:
                return "GAME OVER";
            case GameState::Victory:
                return "VICTORY";
            default:
                return "UNKNOWN";
            }
        }
    } // namespace

    void RuntimeHud::draw(const GameStateSnapshot& snapshot) const {
        if (!snapshot.has_gameplay_scene && snapshot.state == GameState::Playing) {
            return;
        }
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport) {
            return;
        }

        constexpr ImGuiWindowFlags kWindowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs;

        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 16.0f),
            ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (!ImGui::Begin("Runtime HUD", nullptr, kWindowFlags)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Score: %d / %d", snapshot.score, snapshot.collectible_count);
        if (snapshot.player_max_health > 0) {
            ImGui::Text("Health: %d / %d", snapshot.player_health, snapshot.player_max_health);
        }

        if (snapshot.state != GameState::Playing) {
            ImGui::Separator();
            ImGui::TextUnformatted(getStateText(snapshot.state));
        }

        ImGui::End();
    }
} // namespace NexAur
