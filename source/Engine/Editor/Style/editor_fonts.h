#pragma once

#include "Core/Base.h"

#include <imgui.h>

#include <filesystem>

namespace NexAur {
    struct NEXAUR_API EditorFontConfig {
        std::filesystem::path ui_font_path;
        std::filesystem::path mono_font_path;
        std::filesystem::path icon_font_path;
        float ui_font_size = 15.0f;
        float mono_font_size = 14.0f;
        float icon_font_size = 15.0f;
    };

    namespace EditorFonts {
        NEXAUR_API void loadFonts(ImGuiIO& io, const EditorFontConfig& config = EditorFontConfig());
        NEXAUR_API bool hasIconFont();
        NEXAUR_API ImFont* getDefaultFont();
        NEXAUR_API ImFont* getMonoFont();
    } // namespace EditorFonts
} // namespace NexAur
