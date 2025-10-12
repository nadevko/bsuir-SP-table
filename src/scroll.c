#include "main.h"
#include <math.h>

void update_scroll(void) {
#if SMOOTH_SCROLL
  const float scroll_anim_factor = 0.22f; /* fraction per frame (0..1) */
  const float min_scroll_step =
      0.5f; /* minimal pixel change to continue animating */

  if (!g_dragging_vert) {
    float dy = g_scroll_target_y - g_offset_y;
    if (fabsf(dy) > min_scroll_step) {
      g_offset_y += dy * scroll_anim_factor;
    } else {
      g_offset_y = g_scroll_target_y;
    }
  }
  if (!g_dragging_horz) {
    float dx = g_scroll_target_x - g_offset_x;
    if (fabsf(dx) > min_scroll_step) {
      g_offset_x += dx * scroll_anim_factor;
    } else {
      g_offset_x = g_scroll_target_x;
    }
  }
#endif

#if SNAP_VIEW_TO_ROWS
  float max_off_y = SDL_max(0.0f, g_total_grid_h - g_content_h);
  const float snap_eps =
      1.0f; /* small epsilon to decide snapping; choose 1 pixel (safe) */
  if (g_offset_y >= 0.0f && g_offset_y <= snap_eps) {
    g_offset_y = 0.0f;
    g_scroll_target_y = 0.0f;
  }
  if (fabsf(g_offset_y - max_off_y) <= snap_eps) {
    g_offset_y = max_off_y;
    g_scroll_target_y = max_off_y;
  }
#endif
}