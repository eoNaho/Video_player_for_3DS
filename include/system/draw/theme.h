#if !defined(DEF_DRAW_THEME_H)
#define DEF_DRAW_THEME_H

#include "system/draw/draw.h"

// State-rail palette: quiet graphite surfaces, warm white text, and a clear
// aqua rail. Keep the UI opaque and inexpensive to draw on Old 3DS hardware.
#define UI_THEME_COLOR_BACKGROUND	(uint32_t)(0xFF201B14)
#define UI_THEME_COLOR_SURFACE		(uint32_t)(0xFF2A241D)
#define UI_THEME_COLOR_SURFACE_ALT	(uint32_t)(0xFF3A332A)
#define UI_THEME_COLOR_ACCENT		(uint32_t)(0xFFCFD964)
#define UI_THEME_COLOR_ACCENT_WEAK	(uint32_t)(0x50CFD964)
#define UI_THEME_COLOR_TEXT			(uint32_t)(0xFFE7F1F5)
#define UI_THEME_COLOR_SECONDARY_TEXT	(uint32_t)(0xFFC8C4B7)
#define UI_THEME_COLOR_SELECTED_TEXT	(uint32_t)(0xFF242011)
#define UI_THEME_COLOR_MUTED		UI_THEME_COLOR_SURFACE
#define UI_THEME_COLOR_RAIL_OFF		(uint32_t)(0xFF51483D)
#define UI_THEME_COLOR_DANGER		(uint32_t)(0xFF4D5BE8)
#define UI_THEME_COLOR_SUCCESS		(uint32_t)(0xFF70AA6F)
#define UI_THEME_COLOR_WARNING		(uint32_t)(0xFF25D9D5)

// The handheld's existing night-mode setting keeps the interface readable in
// both modes while preserving the same state-rail accent.
#define UI_THEME_COLOR_LIGHT_BACKGROUND	(uint32_t)(0xFFE8F0F2)
#define UI_THEME_COLOR_LIGHT_SURFACE		(uint32_t)(0xFFFFFFFF)
#define UI_THEME_COLOR_LIGHT_SURFACE_ALT	(uint32_t)(0xFFE8E6E0)
#define UI_THEME_COLOR_LIGHT_ACCENT		(uint32_t)(0xFF686C00)
#define UI_THEME_COLOR_LIGHT_ACCENT_WEAK	(uint32_t)(0x50686C00)
#define UI_THEME_COLOR_LIGHT_TEXT		(uint32_t)(0xFF2A251A)
#define UI_THEME_COLOR_LIGHT_SECONDARY_TEXT	(uint32_t)(0xFF5D584B)
#define UI_THEME_COLOR_LIGHT_MUTED		UI_THEME_COLOR_LIGHT_SURFACE
#define UI_THEME_COLOR_LIGHT_SELECTED_TEXT	(uint32_t)(0xFFFFFFFF)
#define UI_THEME_COLOR_LIGHT_RAIL_OFF		(uint32_t)(0xFFC9C7BD)
#define UI_THEME_COLOR_LIGHT_DANGER		(uint32_t)(0xFF2C43AD)
#define UI_THEME_COLOR_LIGHT_SUCCESS		(uint32_t)(0xFF3E6B43)
#define UI_THEME_COLOR_LIGHT_WARNING		(uint32_t)(0xFF4C848F)

#define UI_THEME_COLOR_MODE(is_night, name)	((is_night) ? UI_THEME_COLOR_##name : UI_THEME_COLOR_LIGHT_##name)

#define UI_THEME_TOUCH_MIN			(float)(24)
#define UI_THEME_SPACE_SMALL		(float)(4)
#define UI_THEME_SPACE_MEDIUM		(float)(8)
#define UI_THEME_FONT_HEADING		(float)(16)
#define UI_THEME_FONT_BODY			(float)(14)
#define UI_THEME_FONT_CAPTION		(float)(12)
#define UI_THEME_FONT_BUTTON		(float)(12)

#endif //!defined(DEF_DRAW_THEME_H)
