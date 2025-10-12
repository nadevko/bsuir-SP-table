#include "main.h"
#include <stdlib.h>
#include <string.h>

/* Draw the grid + scrollbars + texts using values from SizeAlloc.
 * GRID_DRAWING_STRATEGY controls behaviour:
 *   1 (default) - draw grid "in relation to content" (as before), but
 *                 vertical lines limited to grid height; draw bottom line after
 * last row. 0           - draw grid filling entire visible area, taking offsets
 * into account.
 *
 * FULL_WIDTH_HORIZ_LINES (new): when GRID_DRAWING_STRATEGY == 1 and this flag
 * is enabled, horizontal separators span content_w (full width). Also draw the
 * right border of the real grid (so the right-side empty column is visible).
 */
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

  /* Vertical separators: draw real ones and extend to the right repeating last
   * column */
  {
    int max_v_separators = g_cols + 100;
    SDL_FRect *vert_rects = malloc(max_v_separators * sizeof(SDL_FRect));
    int vert_count = 0;

    for (int i = 1; i < g_cols; i++) {
      float line_x = view_x - g_offset_x + sa->col_left[i];
      if (line_x + line_w < view_x || line_x > view_x + content_w)
        continue;
      vert_rects[vert_count++] = (SDL_FRect){line_x, view_y, line_w, content_h};
    }

    if (g_cols > 0) {
      float last_col_w = sa->col_widths[g_cols - 1];
      float current_x =
          view_x - g_offset_x + sa->col_left[g_cols - 1] + last_col_w + line_w;
      int guard = 0;
      while (current_x <= view_x + content_w && guard < 1000) {
        vert_rects[vert_count++] =
            (SDL_FRect){current_x, view_y, line_w, content_h};
        current_x += last_col_w + line_w;
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
  /* Horizontal separators BETWEEN rows (only those between rows) */
  SDL_FRect *horz_rects =
      malloc((g_rows) * sizeof(SDL_FRect)); /* +1 for bottom line */
  int horz_count = 0;
  for (int i = 1; i < g_rows; i++) {
    float line_y = view_y - g_offset_y + i * cell_h + (i - 1) * line_w;
    if (line_y >= view_y && line_y <= view_y + content_h) {
      /* width either full content_w (flag) or limited to grid width (old
       * behaviour) */
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

  /* Vertical grid lines — draw only while there is grid (visible grid height),
     not across whole content_h when the table is small. */
  /* compute visible grid top/bottom within content area */
  float grid_top = view_y - g_offset_y;
  float grid_bottom = grid_top + sa->total_grid_h;
  float vis_top = SDL_max(view_y, grid_top);
  float vis_bottom = SDL_min(view_y + content_h, grid_bottom);
  float vis_grid_h = vis_bottom - vis_top;
  if (vis_grid_h < 0.0f)
    vis_grid_h = 0.0f;

  SDL_FRect *vert_rects = malloc(
      (g_cols + 1) * sizeof(SDL_FRect)); /* +1 for potential right border */
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float line_x = view_x - g_offset_x + sa->col_left[i];
    if (line_x >= view_x && line_x <= view_x + content_w) {
      if (vis_grid_h > 0.0f) {
        vert_rects[vert_count++] =
            (SDL_FRect){line_x, vis_top, line_w, vis_grid_h};
      }
    }
  }

  /* If FULL_WIDTH_HORIZ_LINES enabled, draw right border at end of real columns
     so the empty column area to the right is delineated. */
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