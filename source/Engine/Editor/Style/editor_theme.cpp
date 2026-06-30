#include "pch.h"
#include "editor_theme.h"

namespace NexAur {
    namespace {
        ImVec4 color(float r, float g, float b, float a = 1.0f) {
            return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
        }

        EditorTheme makeDefaultTheme() {
            EditorTheme theme;

            theme.colors.background_main = color(15.0f, 20.0f, 25.0f);
            theme.colors.background_panel = color(21.0f, 27.0f, 34.0f);
            theme.colors.background_panel_light = color(27.0f, 35.0f, 44.0f);
            theme.colors.header = color(32.0f, 42.0f, 52.0f);
            theme.colors.header_hovered = color(40.0f, 53.0f, 66.0f);
            theme.colors.header_active = color(47.0f, 64.0f, 80.0f);
            theme.colors.border_subtle = color(42.0f, 52.0f, 64.0f);
            theme.colors.text_primary = color(215.0f, 222.0f, 232.0f);
            theme.colors.text_secondary = color(154.0f, 167.0f, 181.0f);
            theme.colors.accent_blue = color(72.0f, 145.0f, 220.0f);
            theme.colors.accent_cyan = color(54.0f, 199.0f, 218.0f);
            theme.colors.accent_orange = color(232.0f, 155.0f, 70.0f);
            theme.colors.error_red = color(224.0f, 82.0f, 82.0f);
            theme.colors.success_green = color(86.0f, 188.0f, 122.0f);

            theme.metrics.window_padding = ImVec2(7.0f, 6.0f);
            theme.metrics.frame_padding = ImVec2(6.0f, 3.0f);
            theme.metrics.cell_padding = ImVec2(6.0f, 4.0f);
            theme.metrics.item_spacing = ImVec2(6.0f, 4.0f);
            theme.metrics.item_inner_spacing = ImVec2(5.0f, 4.0f);
            theme.metrics.indent_spacing = 14.0f;
            theme.metrics.scrollbar_size = 11.0f;
            theme.metrics.grab_min_size = 8.0f;
            theme.metrics.window_rounding = 0.0f;
            theme.metrics.panel_rounding = 3.0f;
            theme.metrics.frame_rounding = 3.0f;
            theme.metrics.tab_rounding = 3.0f;
            theme.metrics.border_size = 1.0f;

            return theme;
        }
    } // namespace

    namespace EditorThemeTokens {
        const EditorTheme& getDefaultTheme() {
            static const EditorTheme theme = makeDefaultTheme();
            return theme;
        }

        ImVec4 withAlpha(ImVec4 color, float alpha) {
            color.w = alpha;
            return color;
        }
    } // namespace EditorThemeTokens
} // namespace NexAur
