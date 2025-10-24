#include "include/events.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/scroll.h"
#include <SDL3/SDL_events.h>
#include <math.h>

static void ensure_cell_visible_and_scroll(int row, int col) {
  if (!g_col_left || !g_col_widths)
    return;

  if (row >= g_rows) {
    return;
  }

  float cell_x = g_col_left[col];
  float cell_w = (float)g_col_widths[col];
  float line_w = GRID_LINE_WIDTH;
  float cell_h = g_row_height;
  float row_full = cell_h + line_w;

  float cell_y = row * row_full;
  float cell_h_full = cell_h; // только высота ячейки, без линии после

  float left = g_offset_x;
  float right = g_offset_x + g_content_w;
  float top = g_offset_y;
  float bottom = g_offset_y + g_content_h;

  float desired_target_x = g_scroll_target_x;
  float desired_target_y = g_scroll_target_y;

  if (cell_w > g_content_w) {
    desired_target_x = cell_x;
  } else {
    if (cell_x < left) {
      desired_target_x = cell_x;
    } else if (cell_x + cell_w > right) {
      desired_target_x = cell_x + cell_w - g_content_w;
    }
  }

  if (cell_y < top) {
    desired_target_y = cell_y;
  } else if (cell_y + cell_h_full > bottom) {
    desired_target_y = cell_y + cell_h_full - g_content_h;
  }

  float max_target_x = SDL_max(0.0f, g_total_grid_w - g_content_w);
  float max_target_y = SDL_max(0.0f, g_total_grid_h - g_content_h);

  desired_target_x = SDL_clamp(desired_target_x, 0.0f, max_target_x);
  desired_target_y = SDL_clamp(desired_target_y, 0.0f, max_target_y);

  float dx = desired_target_x - g_scroll_target_x;
  float dy = desired_target_y - g_scroll_target_y;

#if SMOOTH_SCROLL
  scroll_add_target(dx, dy);
#else
  scroll_apply_immediate(dx, dy);
#endif
}

static void move_selection_by(int drow, int dcol) {
  int new_r = g_selected_row;
  int new_c = g_selected_col;

  if (new_r < 0 || new_c < 0) {
    new_r = 0;
    new_c = 0;
  } else {
    new_r = new_r + drow;
    new_c = new_c + dcol;
  }

  if (new_r < 0)
    new_r = 0;
  if (new_c < 0)
    new_c = 0;
  if (new_r >= g_rows)
    new_r = g_rows - 1;
  if (new_c >= g_cols)
    new_c = g_cols - 1;

  g_selected_row = new_r;
  g_selected_col = new_c;
  g_selected_index = g_selected_row * g_cols + g_selected_col;

  ensure_cell_visible_and_scroll(g_selected_row, g_selected_col);
}

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
      move_selection_by(-1, 0);
      break;
    case SDLK_DOWN:
      move_selection_by(+1, 0);
      break;
    case SDLK_LEFT:
      move_selection_by(0, -1);
      break;
    case SDLK_RIGHT:
      move_selection_by(0, +1);
      break;
    }
    break;

  case SDL_EVENT_MOUSE_WHEEL: {
    float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
    float dy = scroll_factor * event->wheel.y * SCROLL_SPEED;
    float dx = -scroll_factor * event->wheel.x * SCROLL_SPEED;

#if SMOOTH_SCROLL
    if (g_dragging_vert && g_dragging_horz) {
      scroll_apply_immediate(dx, dy);
    } else if (g_dragging_vert) {
      scroll_apply_immediate(0.0f, dy);
      scroll_add_target(dx, 0.0f);
    } else if (g_dragging_horz) {
      scroll_apply_immediate(dx, 0.0f);
      scroll_add_target(0.0f, dy);
    } else {
      scroll_add_target(dx, dy);
    }
#else
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

      /* Vertical scrollbar hit test */
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

        /* Проверяем клик по всему бару */
        if (mx >= (int)vert_bar_x && mx < (int)(vert_bar_x + SCROLLBAR_WIDTH) &&
            my >= (int)vert_bar_y && my < (int)(vert_bar_y + vert_bar_h)) {

          /* Клик по ползунку - начинаем драг */
          if (my >= (int)thumb_y && my < (int)(thumb_y + thumb_h)) {
            g_dragging_vert = true;
            g_drag_start_pos = my;
            g_drag_start_offset = g_offset_y;
            g_scroll_target_y = g_offset_y;
          } else {
            /* Клик по треку - перемещаем центр ползунка в точку клика */
            float click_pos = my - vert_bar_y;
            float track_h = vert_bar_h - thumb_h;

            /* Вычисляем желаемую позицию центра ползунка */
            float desired_thumb_center = click_pos;

            /* Ограничиваем, чтобы ползунок не выходил за пределы */
            float thumb_center_min = thumb_h / 2.0f;
            float thumb_center_max = vert_bar_h - thumb_h / 2.0f;
            desired_thumb_center = SDL_clamp(
                desired_thumb_center, thumb_center_min, thumb_center_max);

            /* Преобразуем в offset */
            float desired_thumb_top = desired_thumb_center - thumb_h / 2.0f;
            if (track_h > 0.0f && denom > 0.0f) {
              float new_offset = (desired_thumb_top / track_h) * denom;
              /* КРИТИЧЕСКИ ВАЖНО: clamp перед установкой! */
              new_offset = SDL_clamp(new_offset, 0.0f, denom);
              g_offset_y = new_offset;
              g_scroll_target_y = new_offset;
              fprintf(
                  stderr,
                  "VSCROLL CLICK: click_pos=%.2f, new_offset=%.2f, max=%.2f\n",
                  click_pos, new_offset, denom);
              scroll_clamp_all();
            }
          }
          return quit;
        }
      }

      /* Horizontal scrollbar hit test */
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

        /* Проверяем клик по всему бару */
        if (mx >= (int)horz_bar_x && mx < (int)(horz_bar_x + horz_bar_w) &&
            my >= (int)horz_bar_y && my < (int)(horz_bar_y + SCROLLBAR_WIDTH)) {

          /* Клик по ползунку - начинаем драг */
          if (mx >= (int)thumb_x && mx < (int)(thumb_x + thumb_w)) {
            g_dragging_horz = true;
            g_drag_start_pos = mx;
            g_drag_start_offset = g_offset_x;
            g_scroll_target_x = g_offset_x;
          } else {
            /* Клик по треку - перемещаем центр ползунка в точку клика */
            float click_pos = mx - horz_bar_x;
            float track_w = horz_bar_w - thumb_w;

            /* Вычисляем желаемую позицию центра ползунка */
            float desired_thumb_center = click_pos;

            /* Ограничиваем, чтобы ползунок не выходил за пределы */
            float thumb_center_min = thumb_w / 2.0f;
            float thumb_center_max = horz_bar_w - thumb_w / 2.0f;
            desired_thumb_center = SDL_clamp(
                desired_thumb_center, thumb_center_min, thumb_center_max);

            /* Преобразуем в offset */
            float desired_thumb_left = desired_thumb_center - thumb_w / 2.0f;
            if (track_w > 0.0f && denom > 0.0f) {
              float new_offset = (desired_thumb_left / track_w) * denom;
              new_offset = SDL_clamp(new_offset, 0.0f, denom);
              g_offset_x = new_offset;
              g_scroll_target_x = new_offset;
              scroll_clamp_all();
            }
          }
          return quit;
        }
      }

      /* Cell hit-testing */
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

      /* ИСПРАВЛЕНИЕ: проверяем границы строк и попадание в линию */
      if (row < 0 || row >= g_rows || in_row_y >= cell_h) {
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
        ensure_cell_visible_and_scroll(g_selected_row, g_selected_col);
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
      float content_h_local = g_content_h;
      float vert_bar_h = content_h_local;
      float thumb_h =
          SDL_max(10.0f, content_h_local * (content_h_local / g_total_grid_h));
      float max_offset_y = SDL_max(0.0f, g_total_grid_h - content_h_local);
      float track_h = vert_bar_h - thumb_h;
      float dy = event->motion.y - g_drag_start_pos;
      float scroll_factor = (track_h > 0.0f) ? (max_offset_y / track_h) : 0.0f;
      g_offset_y = g_drag_start_offset + dy * scroll_factor;
      g_scroll_target_y = g_offset_y;
      scroll_clamp_all();
    } else if (g_dragging_horz) {
      int win_w_local2;
      SDL_GetWindowSize(g_window, &win_w_local2, NULL);
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