#include <SDL3/SDL_events.h>
#include "include/config.h"
#include "include/globals.h"
#include "include/events.h"

bool handle_events(SDL_Event *event, int win_w_local, int win_h_local) {
  bool quit = false;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    quit = true;
    break;

  case SDL_EVENT_KEY_DOWN:
    switch (event->key.key) {
    case SDLK_W:
      if (event->key.mod & SDL_KMOD_CTRL) {
        quit = true;
      }
      break;
    case SDLK_Q:
      if (event->key.mod & SDL_KMOD_CTRL) {
        quit = true;
      }
      break;
    case SDLK_UP:
      g_offset_y += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
      g_scroll_target_y = g_offset_y;
      break;
    case SDLK_DOWN:
      g_offset_y += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
      g_scroll_target_y = g_offset_y;
      break;
    case SDLK_LEFT:
      g_offset_x += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
      g_scroll_target_x = g_offset_x;
      break;
    case SDLK_RIGHT:
      g_offset_x += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
      g_scroll_target_x = g_offset_x;
      break;
    }
    break;

  case SDL_EVENT_MOUSE_WHEEL: {
#if SMOOTH_SCROLL
    float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
    float dy = scroll_factor * event->wheel.y * SCROLL_SPEED;
    float dx = -scroll_factor * event->wheel.x * SCROLL_SPEED;
    if (!g_dragging_vert) {
      g_scroll_target_y += dy;
    } else {
      g_offset_y += dy;
      g_scroll_target_y = g_offset_y;
    }
    if (!g_dragging_horz) {
      g_scroll_target_x += dx;
    } else {
      g_offset_x += dx;
      g_scroll_target_x = g_offset_x;
    }
#else
    float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
    g_offset_y += scroll_factor * event->wheel.y * SCROLL_SPEED;
    g_offset_x += -scroll_factor * event->wheel.x * SCROLL_SPEED;
    g_scroll_target_x = g_offset_x;
    g_scroll_target_y = g_offset_y;
#endif
  } break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (event->button.button == SDL_BUTTON_LEFT) {
      int mx = event->button.x;
      int my = event->button.y;

#ifdef WITH_BORDER
      float border_local = BORDER_WIDTH;
#else
      float border_local = 0.0f;
#endif
      float view_w_local = win_w_local - 2 * border_local;
      float view_h_local = win_h_local - 2 * border_local;
      float content_w_local =
          view_w_local - (g_need_vert ? SCROLLBAR_WIDTH : 0);
      float content_h_local =
          view_h_local - (g_need_horz ? SCROLLBAR_WIDTH : 0);

      /* Vertical scrollbar thumb hit test */
      if (g_need_vert) {
        float vert_bar_x = border_local + content_w_local;
        float vert_bar_y = border_local;
        float vert_bar_h = content_h_local;
        float thumb_h = SDL_max(10.0f, content_h_local *
                                           (content_h_local / g_total_grid_h));

        float denom = SDL_max(0.0f, g_total_grid_h - content_h_local);
        float thumb_y;
        if (denom <= 0.0f) {
          thumb_y = vert_bar_y;
        } else {
          thumb_y = vert_bar_y + (g_offset_y / denom) * (vert_bar_h - thumb_h);
        }

        if (mx >= vert_bar_x && mx < vert_bar_x + SCROLLBAR_WIDTH &&
            my >= thumb_y && my < thumb_y + thumb_h) {
          g_dragging_vert = true;
          g_drag_start_pos = my;
          g_drag_start_offset = g_offset_y;
          g_scroll_target_y = g_offset_y;
        }
      }

      /* Horizontal scrollbar thumb hit test */
      if (g_need_horz) {
        float horz_bar_x = border_local;
        float horz_bar_y = border_local + content_h_local;
        float horz_bar_w = content_w_local;
        float thumb_w = SDL_max(10.0f, content_w_local *
                                           (content_w_local / g_total_grid_w));

        float denom = SDL_max(0.0f, g_total_grid_w - content_w_local);
        float thumb_x;
        if (denom <= 0.0f) {
          thumb_x = horz_bar_x;
        } else {
          thumb_x = horz_bar_x + (g_offset_x / denom) * (horz_bar_w - thumb_w);
        }

        if (mx >= thumb_x && mx < thumb_x + thumb_w && my >= horz_bar_y &&
            my < horz_bar_y + SCROLLBAR_WIDTH) {
          g_dragging_horz = true;
          g_drag_start_pos = mx;
          g_drag_start_offset = g_offset_x;
          g_scroll_target_x = g_offset_x;
        }
      }
    }
    break;

  case SDL_EVENT_MOUSE_MOTION:
    if (g_dragging_vert) {
      int win_h_local2;
      SDL_GetWindowSize(g_window, NULL, &win_h_local2);
#ifdef WITH_BORDER
      float border_local = BORDER_WIDTH;
#else
      float border_local = 0.0f;
#endif
      float view_h_local = win_h_local2 - 2 * border_local;
      float content_h_local =
          view_h_local - (g_need_horz ? SCROLLBAR_WIDTH : 0);
      float vert_bar_h = content_h_local;
      float thumb_h =
          SDL_max(10.0f, content_h_local * (content_h_local / g_total_grid_h));
      float max_offset_y = SDL_max(0.0f, g_total_grid_h - content_h_local);
      float track_h = vert_bar_h - thumb_h;
      float dy = event->motion.y - g_drag_start_pos;
      float scroll_factor = (track_h > 0.0f) ? (max_offset_y / track_h) : 0.0f;
      g_offset_y = g_drag_start_offset + dy * scroll_factor;
      g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);
      g_scroll_target_y = g_offset_y;
    } else if (g_dragging_horz) {
      int win_w_local2;
      SDL_GetWindowSize(g_window, &win_w_local2, NULL);
#ifdef WITH_BORDER
      float border_local = BORDER_WIDTH;
#else
      float border_local = 0.0f;
#endif
      float view_w_local = win_w_local2 - 2 * border_local;
      float content_w_local =
          view_w_local - (g_need_vert ? SCROLLBAR_WIDTH : 0);
      float horz_bar_w = content_w_local;
      float thumb_w =
          SDL_max(10.0f, content_w_local * (content_w_local / g_total_grid_w));
      float max_offset_x = SDL_max(0.0f, g_total_grid_w - content_w_local);
      float track_w = horz_bar_w - thumb_w;
      float dx = event->motion.x - g_drag_start_pos;
      float scroll_factor = (track_w > 0.0f) ? (max_offset_x / track_w) : 0.0f;
      g_offset_x = g_drag_start_offset + dx * scroll_factor;
      g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
      g_scroll_target_x = g_offset_x;
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (event->button.button == SDL_BUTTON_LEFT) {
      g_dragging_vert = false;
      g_dragging_horz = false;
    }
    break;
  }

  return quit;
}
