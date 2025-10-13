// src/grid.c
#include "include/grid.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/scrollbar.h"
#include "include/utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Draw the grid + texts using values from SizeAlloc.
   Scrollbars moved to scrollbar.c via draw_scrollbars(). */

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
  float line_w = GRID_LINE_WIDTH;
  float cell_h = sa->row_height;
  float row_full = cell_h + line_w;

  /* Clamp offsets to avoid seeing outside content */
  float max_offset_x = SDL_max(0.0f, sa->total_grid_w - content_w);
  float max_offset_y = SDL_max(0.0f, sa->total_grid_h - content_h);

  g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
  g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

  /* Draw grid background */
  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer,
                     &(SDL_FRect){view_x, view_y, content_w, content_h});

  /* Clip to content area for grid drawing */
  SDL_Rect clip_rect = {(int)view_x, (int)view_y, (int)content_w,
                        (int)content_h};
  SDL_SetRenderClipRect(g_renderer, &clip_rect);

#ifdef WITH_GRID
#if GRID_DRAWING_STRATEGY == 0
  /* Horizontal separators tiled to fill view */
  {
    float offset_mod = fmodf(g_offset_y, row_full);
    if (offset_mod < 0.0f)
      offset_mod += row_full;
    float first_row_top_y = view_y - offset_mod;
    int rows_needed = (int)ceilf((content_h + offset_mod) / row_full) + 1;
    SDL_FRect *horz_rects = malloc(rows_needed * sizeof(SDL_FRect));
    int horz_count = 0;
    for (int i = 0; i < rows_needed; i++) {
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
  }

  /* Vertical separators: draw at [col_left[i] - line_w, ... ) */
  {
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
  }

#else
  /* GRID_DRAWING_STRATEGY == 1 (content related) */
  SDL_FRect *horz_rects =
      malloc((g_rows) * sizeof(SDL_FRect)); /* +1 for bottom line */
  int horz_count = 0;
  for (int i = 1; i < g_rows; i++) {
    float line_y = view_y - g_offset_y + i * cell_h + (i - 1) * line_w;
    if (line_y >= view_y && line_y <= view_y + content_h) {
#if FULL_WIDTH_HORIZ_LINES
      float w = content_w;
#else
      float w = SDL_min(content_w, sa->total_grid_w);
#endif
      horz_rects[horz_count++] = (SDL_FRect){view_x, line_y, w, line_w};
    }
  }

  /* bottom line after last row */
  {
    float bottom_y = view_y - g_offset_y + sa->total_grid_h;
    if (bottom_y >= view_y && bottom_y <= view_y + content_h) {
#if FULL_WIDTH_HORIZ_LINES
      float w = content_w;
#else
      float w = SDL_min(content_w, sa->total_grid_w);
#endif
      horz_rects[horz_count++] = (SDL_FRect){view_x, bottom_y, w, line_w};
    }
  }

  /* Vertical separators */
  float grid_top = view_y - g_offset_y;
  float grid_bottom = grid_top + sa->total_grid_h;
  float vis_top = SDL_max(view_y, grid_top);
  float vis_bottom = SDL_min(view_y + content_h, grid_bottom);
  float vis_grid_h = vis_bottom - vis_top;
  if (vis_grid_h < 0.0f)
    vis_grid_h = 0.0f;

  SDL_FRect *vert_rects = malloc((g_cols + 1) * sizeof(SDL_FRect));
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float sep_x = view_x - g_offset_x + sa->col_left[i] - line_w;
    if (sep_x >= view_x && sep_x <= view_x + content_w) {
      if (vis_grid_h > 0.0f) {
        vert_rects[vert_count++] =
            (SDL_FRect){sep_x, vis_top, line_w, vis_grid_h};
      }
    }
  }

#if FULL_WIDTH_HORIZ_LINES
  if (g_cols > 0 && vis_grid_h > 0.0f) {
    float right_x = view_x - g_offset_x + sa->col_left[g_cols - 1] +
                    sa->col_widths[g_cols - 1];
    if (right_x >= view_x && right_x <= view_x + content_w) {
      vert_rects[vert_count++] =
          (SDL_FRect){right_x, vis_top, line_w, vis_grid_h};
    }
  }
#endif

  if (horz_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, horz_rects, horz_count);
  }
  if (vert_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, vert_rects, vert_count);
  }
  free(horz_rects);
  free(vert_rects);
#endif
#endif /* WITH_GRID */

  /* Draw selection border (outer edge lies on cell edge and border goes inward
   * HIGHLIGHT_BORDER_WIDTH) */
  if (g_selected_index >= 0 && g_selected_row >= 0 && g_selected_col >= 0 &&
      g_selected_row < g_rows && g_selected_col < g_cols && g_col_left &&
      g_col_widths) {

    float cell_x = view_x - g_offset_x + g_col_left[g_selected_col];
    float cell_y = view_y - g_offset_y + g_selected_row * row_full;
    float cell_wd = (float)g_col_widths[g_selected_col];
    float cell_hh = sa->row_height;

    float bw = (float)HIGHLIGHT_BORDER_WIDTH;
    float max_bw = SDL_min(cell_wd, cell_hh) / 2.0f;
    if (bw > max_bw)
      bw = max_bw;
    if (bw <= 0.0f)
      bw = 1.0f;

    if (cell_x + cell_wd >= view_x && cell_x <= view_x + content_w &&
        cell_y + cell_hh >= view_y && cell_y <= view_y + content_h) {

      SDL_SetRenderDrawColour(g_renderer, HIGHLIGHT_BORDER_COLOUR);

      /* Top */
      SDL_RenderFillRect(g_renderer, &(SDL_FRect){cell_x, cell_y, cell_wd, bw});
      /* Left */
      SDL_RenderFillRect(g_renderer, &(SDL_FRect){cell_x, cell_y, bw, cell_hh});
      /* Right */
      SDL_RenderFillRect(
          g_renderer, &(SDL_FRect){cell_x + cell_wd - bw, cell_y, bw, cell_hh});
      /* Bottom */
      SDL_RenderFillRect(
          g_renderer, &(SDL_FRect){cell_x, cell_y + cell_hh - bw, cell_wd, bw});
    }
  }

  /* Draw cell texts (only visible ones) -- same for both strategies */
  for (int r = 0; r < g_rows; r++) {
    float cell_y = view_y - g_offset_y + r * row_full;
    if (cell_y + cell_h < view_y || cell_y > view_y + content_h)
      continue;
    for (int c = 0; c < g_cols; c++) {
      float cell_x = view_x - g_offset_x + sa->col_left[c];
      if (cell_x + sa->col_widths[c] < view_x || cell_x > view_x + content_w)
        continue;
      if (!g_grid[r][c].text || g_grid[r][c].text[0] == '\0')
        continue;
      size_t len = strlen(g_grid[r][c].text);
      SDL_Surface *label_surface =
          TTF_RenderText_LCD(g_font, g_grid[r][c].text, len, TEXT_FONT_COLOUR,
                             GRID_BACKGROUND_COLOUR);
      if (!label_surface)
        continue;
      SDL_Texture *label_texture =
          SDL_CreateTextureFromSurface(g_renderer, label_surface);
      if (!label_texture) {
        SDL_DestroySurface(label_surface);
        continue;
      }

      float padding_x = 0, padding_y = 0;
      int text_w = label_surface->w;
      int text_h = label_surface->h;
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
      SDL_DestroySurface(label_surface);
      SDL_DestroyTexture(label_texture);
    }
  }

  SDL_SetRenderClipRect(g_renderer, NULL);

  /* Draw scrollbars (выделенная ответственность) */
  draw_scrollbars(sa);

  SDL_RenderPresent(g_renderer);
}
