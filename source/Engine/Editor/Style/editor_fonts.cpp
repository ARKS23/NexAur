#include "pch.h"
#include "editor_fonts.h"

#include <system_error>

namespace NexAur::EditorFonts {
    namespace {
        ImFont* g_default_font = nullptr;
        ImFont* g_mono_font = nullptr;
        bool g_has_icon_font = false;

        bool canLoadFont(const std::filesystem::path& path) {
            if (path.empty()) {
                return false;
            }

            std::error_code ec;
            return std::filesystem::exists(path, ec) &&
                std::filesystem::is_regular_file(path, ec) &&
                !ec;
        }
    } // namespace

    void loadFonts(ImGuiIO& io, const EditorFontConfig& config) {
        g_default_font = io.FontDefault;
        g_mono_font = io.FontDefault;
        g_has_icon_font = false;

        if (canLoadFont(config.ui_font_path)) {
            ImFont* loaded_font = io.Fonts->AddFontFromFileTTF(
                config.ui_font_path.string().c_str(),
                config.ui_font_size);
            if (loaded_font) {
                g_default_font = loaded_font;
            }
        }

        if (canLoadFont(config.mono_font_path)) {
            ImFont* loaded_font = io.Fonts->AddFontFromFileTTF(
                config.mono_font_path.string().c_str(),
                config.mono_font_size);
            if (loaded_font) {
                g_mono_font = loaded_font;
            }
        }

        // Icon font merge is intentionally left as a centralized hook.
        // Actual icon font assets require an explicit license review.
        g_has_icon_font = false;
    }

    bool hasIconFont() {
        return g_has_icon_font;
    }

    ImFont* getDefaultFont() {
        return g_default_font;
    }

    ImFont* getMonoFont() {
        return g_mono_font;
    }
} // namespace NexAur::EditorFonts
