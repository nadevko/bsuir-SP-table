/* src/grid.c */
#include "include/grid.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/scrollbar.h"
#include "include/table_model.h"
#include "include/utils.h"
#include "include/virtual_scroll.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void draw_with_alloc(const SizeAlloc *sa) {
  int win_w, win_h;
  SDL_GetWindowSize(g_window, &win_w, &win_h);

#ifdef WITH_BORDER
  float border = BORDER_WIDTH;
#else
  float border = 0.0f;
#endif

  float view_x = g_view_x;
  float view_y = g_view_y;
  float content_w = sa->content_w;
  float content_h = sa->content_h;

  float clip_w = sa->content_w + (sa->need_vert ? SCROLLBAR_WIDTH : 0.0f);
  float clip_h = sa->content_h + (sa->need_horz ? SCROLLBAR_WIDTH : 0.0f);

  float line_w = GRID_LINE_WIDTH;
  float cell_h = sa->row_height;
  float row_full = cell_h + line_w;

  float max_offset_x = SDL_max(0.0f, sa->total_grid_w - content_w);
  float max_offset_y = SDL_max(0.0f, sa->total_grid_h - content_h);

  g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
  g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer,
                     &(SDL_FRect){view_x, view_y, content_w, content_h});

  SDL_Rect clip_rect = {(int)view_x, (int)view_y, (int)clip_w, (int)clip_h};
  SDL_SetRenderClipRect(g_renderer, &clip_rect);

#ifdef WITH_GRID
  float offset_mod = fmodf(g_offset_y, row_full);
  if (offset_mod < 0.0f)
    offset_mod += row_full;
  float first_row_top_y = view_y - offset_mod;
  int rows_needed = (int)ceilf((content_h + offset_mod) / row_full) + 1;
  SDL_FRect *horz_rects = malloc(rows_needed * sizeof(SDL_FRect));
  int horz_count = 0;

  int first_visible_row = (int)floorf(g_offset_y / row_full);

  int total_rows = g_vscroll ? g_vscroll->total_virtual_rows : g_rows;

  for (int i = 0; i < rows_needed; i++) {
    int current_row = first_visible_row + i;
    if (current_row >= total_rows)
      break;

    float sep_y = first_row_top_y + i * row_full + cell_h;
    if (sep_y + line_w < view_y || sep_y > view_y + content_h)
      continue;
    horz_rects[horz_count++] = (SDL_FRect){view_x, sep_y, content_w, line_w};
  }
  if (horz_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, horz_rects, horz_count);
  }
  free(horz_rects);

  int max_v_separators = g_cols + 100;
  SDL_FRect *vert_rects = malloc(max_v_separators * sizeof(SDL_FRect));
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float sep_x = view_x - g_offset_x + sa->col_left[i] - line_w;
    if (sep_x + line_w < view_x || sep_x > view_x + content_w)
      continue;
    vert_rects[vert_count++] = (SDL_FRect){sep_x, view_y, line_w, content_h};
  }
  if (g_cols > 0) {
    float last_col_w = sa->col_widths[g_cols - 1];
    float current_sep_x = view_x - g_offset_x + sa->col_left[g_cols - 1] +
                          sa->col_widths[g_cols - 1];
    int guard = 0;
    while (current_sep_x <= view_x + content_w && guard < 10000) {
      if (!(current_sep_x + line_w < view_x ||
            current_sep_x > view_x + content_w)) {
        vert_rects[vert_count++] =
            (SDL_FRect){current_sep_x, view_y, line_w, content_h};
      }
      current_sep_x += last_col_w + line_w;
      guard++;
    }
  }
  if (vert_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, vert_rects, vert_count);
  }
  free(vert_rects);
#endif

  if (g_selected_index >= 0 && g_selected_row >= 0 && g_selected_col >= 0 &&
      g_selected_row < (g_vscroll ? g_vscroll->total_virtual_rows : g_rows) &&
      g_selected_col < g_cols && g_col_left && g_col_widths) {

    int first_visible_row = (int)floorf(g_offset_y / row_full);
    float offset_within_first = fmodf(g_offset_y, row_full);
    if (offset_within_first < 0.0f)
      offset_within_first += row_full;
    int row_offset_from_first = g_selected_row - first_visible_row;

    float cell_x = view_x - g_offset_x + g_col_left[g_selected_col];
    float cell_y =
        view_y + row_offset_from_first * row_full - offset_within_first;
    float cell_wd = (float)g_col_widths[g_selected_col];
    float cell_h = sa->row_height;

    float bw = (float)HIGHLIGHT_BORDER_WIDTH;
    float max_bw = SDL_min(cell_wd, cell_h) / 2.0f;
    if (bw > max_bw)
      bw = max_bw;
    if (bw <= 0.0f)
      bw = 1.0f;

    if (cell_x + cell_wd >= view_x && cell_x <= view_x + content_w &&
        cell_y + cell_h >= view_y && cell_y <= view_y + content_h) {

      SDL_SetRenderDrawColour(g_renderer, HIGHLIGHT_BORDER_COLOUR);
      SDL_RenderFillRect(g_renderer, &(SDL_FRect){cell_x, cell_y, cell_wd, bw});
      SDL_RenderFillRect(g_renderer, &(SDL_FRect){cell_x, cell_y, bw, cell_h});
      SDL_RenderFillRect(
          g_renderer, &(SDL_FRect){cell_x + cell_wd - bw, cell_y, bw, cell_h});
      SDL_RenderFillRect(
          g_renderer, &(SDL_FRect){cell_x, cell_y + cell_h - bw, cell_wd, bw});
    }
  }

  if (g_vscroll) {
    if (g_vscroll->needs_reload)
      goto skip_text_render;

    int first_visible_row = (int)floorf(g_offset_y / row_full);
    float offset_within_first = fmodf(g_offset_y, row_full);
    if (offset_within_first < 0.0f)
      offset_within_first += row_full;

    int table_rows = g_table ? table_get_row_count(g_table) : 0;

    for (int virtual_row = first_visible_row;
         virtual_row < first_visible_row + (int)ceilf(content_h / row_full) + 1;
         virtual_row++) {

      if (virtual_row < 0 || virtual_row >= table_rows + 1)
        continue;

      int row_offset_from_first = virtual_row - first_visible_row;
      float cell_y =
          view_y + row_offset_from_first * row_full - offset_within_first;

      if (cell_y + cell_h < view_y || cell_y > view_y + content_h)
        continue;

      for (int c = 0; c < g_cols; c++) {
        float cell_x = view_x - g_offset_x + sa->col_left[c];

        if (cell_x + sa->col_widths[c] < view_x || cell_x > view_x + content_w)
          continue;

        char *cell_text = NULL;
        int text_w = 0, text_h = 0;

        if (virtual_row == 0) {
          /* Header row */
          cell_text = table_get_header(g_table, c);
        } else {
          /* Data row */
          cell_text = table_get_cell(g_table, virtual_row - 1, c);
        }

        if (!cell_text || cell_text[0] == '\0') {
          free(cell_text);
          continue;
        }

        size_t len = strlen(cell_text);
        SDL_Surface *label_surface = TTF_RenderText_LCD(
            g_font, cell_text, len, TEXT_FONT_COLOUR, GRID_BACKGROUND_COLOUR);
        if (!label_surface) {
          free(cell_text);
          continue;
        }

        text_w = label_surface->w;
        text_h = label_surface->h;

        SDL_Texture *label_texture =
            SDL_CreateTextureFromSurface(g_renderer, label_surface);
        SDL_DestroySurface(label_surface);
        if (!label_texture) {
          free(cell_text);
          continue;
        }

        float padding_x = 0, padding_y = 0;

#if TEXT_FONT_POSITION_HORIZONTAL == LEFT
        padding_x = CELL_PADDING;
#elif TEXT_FONT_POSITION_HORIZONTAL == CENTER
        padding_x = (sa->col_widths[c] - text_w) / 2.0f;
#elif TEXT_FONT_POSITION_HORIZONTAL == RIGHT
        padding_x = sa->col_widths[c] - text_w - CELL_PADDING;
#endif

#if TEXT_FONT_POSITION_VERTICAL == TOP
        padding_y = CELL_PADDING;
#elif TEXT_FONT_POSITION_VERTICAL == CENTER
        padding_y = (cell_h - text_h) / 2.0f;
#elif TEXT_FONT_POSITION_VERTICAL == BOTTOM
        padding_y = cell_h - text_h - CELL_PADDING;
#endif

        SDL_RenderTexture(g_renderer, label_texture, NULL,
                          &(SDL_FRect){cell_x + padding_x, cell_y + padding_y,
                                       (float)text_w, (float)text_h});

        SDL_DestroyTexture(label_texture);
        free(cell_text);
      }
    }
  }

skip_text_render:

  SDL_SetRenderClipRect(g_renderer, NULL);

  draw_scrollbars(sa);

  SDL_RenderPresent(g_renderer);
}