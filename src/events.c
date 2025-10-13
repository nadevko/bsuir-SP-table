// src/events.c
#include "include/events.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/scroll.h"
#include <SDL3/SDL_events.h>
#include <math.h>

/*
  Обработчик событий:
  - использует scroll_add_target/scroll_apply_immediate вместо прямой
  модификации глобалей, чтобы логика прокрутки оставалась в scroll.c.
  - остальная логика hit-testing/drag/selection оставлена прежней.
*/

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
      /* стрелки должны перемещать как раньше — применяем немедленно и
       * синхронизируем цель */
      scroll_apply_immediate(0.0f,
                             (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED));
      break;
    case SDLK_DOWN:
      scroll_apply_immediate(0.0f,
                             (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED));
      break;
    case SDLK_LEFT:
      scroll_apply_immediate((NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED),
                             0.0f);
      break;
    case SDLK_RIGHT:
      scroll_apply_immediate((NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED),
                             0.0f);
      break;
    }
    break;

  case SDL_EVENT_MOUSE_WHEEL: {
    /* dx/dy — в пикселях (ориентировочно), сохраняем прежнюю знаковую логику */
    float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
    float dy = scroll_factor * event->wheel.y * SCROLL_SPEED;
    float dx = -scroll_factor * event->wheel.x * SCROLL_SPEED;

#if SMOOTH_SCROLL
    /* Если включена плавность — при колесике меняем ТОЛЬКО цель (если не
       тащат), иначе при drag'e (перетаскивании ползунка) применяем немедленно.
     */
    if (g_dragging_vert) {
      scroll_apply_immediate(0.0f, dy);
    } else {
      scroll_add_target(0.0f, dy);
    }

    if (g_dragging_horz) {
      scroll_apply_immediate(dx, 0.0f);
    } else {
      scroll_add_target(dx, 0.0f);
    }
#else
    /* Немедленно применяем и синхронизируем цель */
    scroll_apply_immediate(dx, dy);
#endif
  } break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (event->button.button == SDL_BUTTON_LEFT) {
      int mx = event->button.x;
      int my = event->button.y;

      float view_x_local = g_view_x;
      float view_y_local = g_view_y;
      float content_w_local = g_content_w;
      float content_h_local = g_content_h;

      /* Vertical scrollbar thumb hit test */
      if (g_need_vert) {
        float vert_bar_x = view_x_local + content_w_local;
        float vert_bar_y = view_y_local;
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
        if (mx >= (int)vert_bar_x && mx < (int)(vert_bar_x + SCROLLBAR_WIDTH) &&
            my >= (int)thumb_y && my < (int)(thumb_y + thumb_h)) {
          g_dragging_vert = true;
          g_drag_start_pos = my;
          g_drag_start_offset = g_offset_y;
          g_scroll_target_y = g_offset_y;
          return quit;
        }
      }

      /* Horizontal scrollbar thumb hit test */
      if (g_need_horz) {
        float horz_bar_x = view_x_local;
        float horz_bar_y = view_y_local + content_h_local;
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
        if (mx >= (int)thumb_x && mx < (int)(thumb_x + thumb_w) &&
            my >= (int)horz_bar_y && my < (int)(horz_bar_y + SCROLLBAR_WIDTH)) {
          g_dragging_horz = true;
          g_drag_start_pos = mx;
          g_drag_start_offset = g_offset_x;
          g_scroll_target_x = g_offset_x;
          return quit;
        }
      }

      /* Cell hit-testing (как раньше) */
      if (mx < (int)view_x_local ||
          mx >= (int)(view_x_local + content_w_local) ||
          my < (int)view_y_local ||
          my >= (int)(view_y_local + content_h_local)) {
        g_selected_row = -1;
        g_selected_col = -1;
        g_selected_index = -1;
        return quit;
      }

      float virtual_x = (float)(mx - view_x_local) + g_offset_x;
      float virtual_y = (float)(my - view_y_local) + g_offset_y;

      float line_w = GRID_LINE_WIDTH;
      float cell_h = g_row_height;
      float row_full = cell_h + line_w;

      if (virtual_y < 0.0f) {
        g_selected_row = -1;
        g_selected_col = -1;
        g_selected_index = -1;
        return quit;
      }
      int row = (int)(virtual_y / row_full);
      float in_row_y = fmodf(virtual_y, row_full);
      if (in_row_y < 0.0f)
        in_row_y += row_full;

      if (in_row_y >= cell_h || row < 0 || row >= g_rows) {
        g_selected_row = -1;
        g_selected_col = -1;
        g_selected_index = -1;
        return quit;
      }

      if (!g_col_left || !g_col_widths) {
        g_selected_row = -1;
        g_selected_col = -1;
        g_selected_index = -1;
        return quit;
      }

      int found_col = -1;
      for (int c = 0; c < g_cols; c++) {
        float col_l = g_col_left[c];
        float col_w = (float)g_col_widths[c];
        if (virtual_x >= col_l && virtual_x < col_l + col_w) {
          found_col = c;
          break;
        }
        if (c < g_cols - 1) {
          float sep_start = col_l + col_w;
          float sep_end = sep_start + line_w;
          if (virtual_x >= sep_start && virtual_x < sep_end) {
            found_col = -1;
            break;
          }
        }
      }

      if (found_col >= 0) {
        g_selected_row = row;
        g_selected_col = found_col;
        g_selected_index = g_selected_row * g_cols + g_selected_col;
      } else {
        g_selected_row = -1;
        g_selected_col = -1;
        g_selected_index = -1;
      }
    }
    break;

  case SDL_EVENT_MOUSE_MOTION:
    if (g_dragging_vert) {
      int win_h_local2;
      SDL_GetWindowSize(g_window, NULL, &win_h_local2);
      float view_h_local = g_view_h;
      float content_h_local = g_content_h;
      float vert_bar_h = content_h_local;
      float thumb_h =
          SDL_max(10.0f, content_h_local * (content_h_local / g_total_grid_h));
      float max_offset_y = SDL_max(0.0f, g_total_grid_h - content_h_local);
      float track_h = vert_bar_h - thumb_h;
      float dy = event->motion.y - g_drag_start_pos;
      float scroll_factor = (track_h > 0.0f) ? (max_offset_y / track_h) : 0.0f;
      g_offset_y = g_drag_start_offset + dy * scroll_factor;
      /* при перетаскивании синхронизируем цель */
      g_scroll_target_y = g_offset_y;
      scroll_clamp_all();
    } else if (g_dragging_horz) {
      int win_w_local2;
      SDL_GetWindowSize(g_window, &win_w_local2, NULL);
      float view_w_local = g_view_w;
      float content_w_local = g_content_w;
      float horz_bar_w = content_w_local;
      float thumb_w =
          SDL_max(10.0f, content_w_local * (content_w_local / g_total_grid_w));
      float max_offset_x = SDL_max(0.0f, g_total_grid_w - content_w_local);
      float track_w = horz_bar_w - thumb_w;
      float dx = event->motion.x - g_drag_start_pos;
      float scroll_factor = (track_w > 0.0f) ? (max_offset_x / track_w) : 0.0f;
      g_offset_x = g_drag_start_offset + dx * scroll_factor;
      g_scroll_target_x = g_offset_x;
      scroll_clamp_all();
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
