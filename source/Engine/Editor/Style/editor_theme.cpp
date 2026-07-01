#include "pch.h"
#include "editor_theme.h"

#include <cmath>
#include <string>

namespace NexAur {
    namespace {
        float srgbToLinear(float value) {
            if (value <= 0.04045f) {
                return value / 12.92f;
            }

            return std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        ImVec4 color(float r, float g, float b, float a = 1.0f) {
            return EditorThemeTokens::fromSrgb(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a));
        }

        EditorTheme makeModernBlackTheme() {
            EditorTheme theme;

            theme.colors.background_main = color(11.0f, 13.0f, 15.0f);
            theme.colors.background_panel = color(17.0f, 19.0f, 21.0f);
            theme.colors.background_panel_light = color(23.0f, 26.0f, 29.0f);
            theme.colors.background_input = color(8.0f, 10.0f, 12.0f);
            theme.colors.header = color(29.0f, 32.0f, 36.0f);
            theme.colors.header_hovered = color(37.0f, 42.0f, 48.0f);
            theme.colors.header_active = color(43.0f, 50.0f, 58.0f);
            theme.colors.border_subtle = color(42.0f, 47.0f, 53.0f);
            theme.colors.text_primary = color(214.0f, 217.0f, 221.0f);
            theme.colors.text_secondary = color(139.0f, 146.0f, 154.0f);
            theme.colors.accent_blue = color(45.0f, 140.0f, 255.0f);
            theme.colors.accent_cyan = color(22.0f, 183.0f, 200.0f);
            theme.colors.accent_orange = color(209.0f, 138.0f, 47.0f);
            theme.colors.error_red = color(214.0f, 73.0f, 73.0f);
            theme.colors.success_green = color(83.0f, 181.0f, 118.0f);

            theme.metrics.window_padding = ImVec2(6.0f, 6.0f);
            theme.metrics.frame_padding = ImVec2(6.0f, 3.0f);
            theme.metrics.cell_padding = ImVec2(6.0f, 3.0f);
            theme.metrics.item_spacing = ImVec2(6.0f, 4.0f);
            theme.metrics.item_inner_spacing = ImVec2(5.0f, 4.0f);
            theme.metrics.indent_spacing = 14.0f;
            theme.metrics.scrollbar_size = 10.0f;
            theme.metrics.grab_min_size = 7.0f;
            theme.metrics.window_rounding = 0.0f;
            theme.metrics.panel_rounding = 2.0f;
            theme.metrics.frame_rounding = 4.0f;
            theme.metrics.tab_rounding = 2.0f;
            theme.metrics.border_size = 1.0f;

            return theme;
        }

        std::string& activeThemeVariantStorage() {
            static std::string active_theme_variant(EditorThemeTokens::ModernBlack);
            return active_theme_variant;
        }
    } // namespace

    namespace EditorThemeTokens {
        void setActiveThemeVariant(std::string_view variant) {
            activeThemeVariantStorage() = variant.empty()
                ? std::string(ModernBlack)
                : std::string(variant);
        }

        std::string_view getActiveThemeVariant() {
            return activeThemeVariantStorage();
        }

        const EditorTheme& getDefaultTheme() {
            static const EditorTheme theme = makeModernBlackTheme();
            return theme;
        }

        ImVec4 withAlpha(ImVec4 color, float alpha) {
            color.w = alpha;
            return color;
        }

        ImVec4 fromSrgb(ImVec4 color) {
            return ImVec4(
                srgbToLinear(color.x),
                srgbToLinear(color.y),
                srgbToLinear(color.z),
                color.w);
        }
    } // namespace EditorThemeTokens
} // namespace NexAur
