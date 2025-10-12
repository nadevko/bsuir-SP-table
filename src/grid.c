#include "main.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
 * sizeAllocate:
 *  - given the current window size and the global grid/font state,
 *    compute column widths, column left offsets, total grid width/height,
 *    whether horizontal/vertical scrollbars are needed, and content sizes.
 *  - returns a SizeAlloc struct; col_widths and col_left are malloc'ed and
 *    must be freed by the caller (free(sa.col_widths); free(sa.col_left);).
 */
SizeAlloc sizeAllocate(int win_w, int win_h) {
  SizeAlloc sa;
  sa.col_widths = NULL;
  sa.col_left = NULL;
  sa.need_horz = false;
  sa.need_vert = false;
  sa.total_grid_w = 0.0f;
  sa.total_grid_h = 0.0f;
  sa.content_w = 0.0f;
  sa.content_h = 0.0f;

#ifdef WITH_BORDER
  float border = BORDER_WIDTH;
#else
  float border = 0.0f;
#endif

  float view_x = border;
  float view_y = border;
  float view_w = win_w - 2 * border;
  float view_h = win_h - 2 * border;

  int font_height = TTF_GetFontHeight(g_font);
  float cell_h = font_height + 2 * CELL_PADDING;
  float line_w = GRID_LINE_WIDTH;

  /* total vertical size of grid (all rows + separators) */
  sa.total_grid_h =
      g_rows * cell_h + (g_rows > 0 ? (g_rows - 1) * line_w : 0.0f);

  /* Compute max text width per column */
  int *max_col_w = calloc(g_cols, sizeof(int));
  for (int c = 0; c < g_cols; c++)
    for (int r = 0; r < g_rows; r++)
      if (g_grid[r][c].text && g_grid[r][c].text[0] != '\0')
        if (g_grid[r][c].text_width > max_col_w[c])
          max_col_w[c] = g_grid[r][c].text_width;

  /* Compute column widths */
  sa.col_widths = malloc(g_cols * sizeof(int));
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++) {
    sa.col_widths[c] = max_col_w[c] + 2 * CELL_PADDING;
    sa.total_grid_w += sa.col_widths[c];
  }
  sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);

  free(max_col_w);

  /* Interdependency between scrollbars: iteratively compute need_horz/need_vert
   */
  bool need_horz = false;
  bool need_vert = false;
  bool changed = true;
  while (changed) {
    changed = false;
    float temp_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
    float temp_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);
    bool new_horz = sa.total_grid_w > temp_w;
    bool new_vert = sa.total_grid_h > temp_h;
    if (new_horz != need_horz) {
      need_horz = new_horz;
      changed = true;
    }
    if (new_vert != need_vert) {
      need_vert = new_vert;
      changed = true;
    }
  }

  sa.need_horz = need_horz;
  sa.need_vert = need_vert;

  /* content area size after accounting for scrollbars */
  sa.content_w = view_w - (sa.need_vert ? SCROLLBAR_WIDTH : 0.0f);
  sa.content_h = view_h - (sa.need_horz ? SCROLLBAR_WIDTH : 0.0f);

  /* Column left positions (virtual) */
  sa.col_left = malloc(g_cols * sizeof(float));
  if (g_cols > 0)
    sa.col_left[0] = 0.0f;
  for (int c = 1; c < g_cols; c++) {
    sa.col_left[c] = sa.col_left[c - 1] + sa.col_widths[c - 1] + line_w;
  }

  /* store view area for event handlers usage (set in main loop) */
  g_view_x = view_x;
  g_view_y = view_y;
  g_view_w = view_w;
  g_view_h = view_h;

  return sa;
}

/* Draw the grid + scrollbars + texts using values from SizeAlloc.
 * The function does not free sa.col_widths / sa.col_left (caller frees).
 */
void draw_with_alloc(const SizeAlloc *sa) {
  /* Use the global renderer and g_font. Sa contains precomputed sizes. */
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
  int font_height = TTF_GetFontHeight(g_font);
  float cell_h = font_height + 2 * CELL_PADDING;

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

  /* Draw vertical scrollbar if needed */
  float vert_thumb_h = 0.0f, vert_thumb_y = 0.0f;
  if (sa->need_vert) {
    float vert_bar_x = view_x + content_w;
    float vert_bar_y = view_y;
    float vert_bar_h = content_h;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_bar_y,
                                                SCROLLBAR_WIDTH, vert_bar_h});

    vert_thumb_h = SDL_max(10.0f, content_h * (content_h / sa->total_grid_h));

    /* Avoid division by zero if max_offset_y == 0 */
    float max_offset_y_local = SDL_max(0.0f, sa->total_grid_h - content_h);
    if (max_offset_y_local <= 0.0f) {
      vert_thumb_y = vert_bar_y;
    } else {
      float track_h = vert_bar_h - vert_thumb_h;
      vert_thumb_y = vert_bar_y + (g_offset_y / max_offset_y_local) * track_h;
    }

    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_thumb_y,
                                                SCROLLBAR_WIDTH, vert_thumb_h});
  }

  /* Draw horizontal scrollbar if needed */
  float horz_thumb_w = 0.0f, horz_thumb_x = 0.0f;
  if (sa->need_horz) {
    float horz_bar_x = view_x;
    float horz_bar_y = view_y + content_h;
    float horz_bar_w = content_w;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_bar_x, horz_bar_y,
                                                horz_bar_w, SCROLLBAR_WIDTH});

    horz_thumb_w = SDL_max(10.0f, content_w * (content_w / sa->total_grid_w));

    float max_offset_x_local = SDL_max(0.0f, sa->total_grid_w - content_w);
    if (max_offset_x_local <= 0.0f) {
      horz_thumb_x = horz_bar_x;
    } else {
      float track_w = horz_bar_w - horz_thumb_w;
      horz_thumb_x = horz_bar_x + (g_offset_x / max_offset_x_local) * track_w;
    }

    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_thumb_x, horz_bar_y,
                                                horz_thumb_w, SCROLLBAR_WIDTH});
  }

  /* Draw corner if both scrollbars */
  if (sa->need_vert && sa->need_horz) {
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer,
                       &(SDL_FRect){view_x + content_w, view_y + content_h,
                                    SCROLLBAR_WIDTH, SCROLLBAR_WIDTH});
  }

  /* Clip to content area for grid drawing */
  SDL_Rect clip_rect = {(int)view_x, (int)view_y, (int)content_w,
                        (int)content_h};
  SDL_SetRenderClipRect(g_renderer, &clip_rect);

#ifdef WITH_GRID
  /* Collect horizontal grid lines (only visible ones) */
  SDL_FRect *horz_rects = malloc((g_rows - 1) * sizeof(SDL_FRect));
  int horz_count = 0;
  for (int i = 1; i < g_rows; i++) {
    float line_y = view_y - g_offset_y + i * cell_h + (i - 1) * line_w;
    if (line_y >= view_y && line_y <= view_y + content_h) {
      horz_rects[horz_count++] = (SDL_FRect){view_x, line_y, content_w, line_w};
    }
  }

  /* Collect vertical grid lines (only visible ones) */
  SDL_FRect *vert_rects = malloc((g_cols - 1) * sizeof(SDL_FRect));
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float line_x = view_x - g_offset_x + sa->col_left[i];
    if (line_x >= view_x && line_x <= view_x + content_w) {
      vert_rects[vert_count++] = (SDL_FRect){line_x, view_y, line_w, content_h};
    }
  }

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

  /* Draw cell texts (only visible ones) */
  for (int r = 0; r < g_rows; r++) {
    float cell_y = view_y - g_offset_y + r * (cell_h + line_w);
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
          TTF_RenderText_LCD(g_font, g_grid[r][c].text, len, CELL_TEXT_COLOUR,
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

#if CELL_TEXT_POSITION_HORIZONTAL == LEFT
      padding_x = CELL_PADDING;
#elif CELL_TEXT_POSITION_HORIZONTAL == CENTER
      padding_x = (sa->col_widths[c] - text_w) / 2.0f;
#elif CELL_TEXT_POSITION_HORIZONTAL == RIGHT
      padding_x = sa->col_widths[c] - text_w - CELL_PADDING;
#endif

#if CELL_TEXT_POSITION_VERTICAL == TOP
      padding_y = CELL_PADDING;
#elif CELL_TEXT_POSITION_VERTICAL == CENTER
      padding_y = (cell_h - text_h) / 2.0f;
#elif CELL_TEXT_POSITION_VERTICAL == BOTTOM
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
  SDL_RenderPresent(g_renderer);
}
