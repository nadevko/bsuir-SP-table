#include "main.h"
#include <math.h>
#include <stdlib.h>

/*
 * sizeAllocate:
 *  - compute column widths, whether scrollbars are needed, content sizes
 *  - if SNAP_VIEW_TO_ROWS enabled, adjust row_height so visible area
 * divides exactly into integer number of rows (no half-row at top/end)
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
  sa.row_height = 0.0f;

#ifdef WITH_BORDER
  float border = BORDER_WIDTH;
#else
  float border = 0.0f;
#endif

  float view_x = border;
  float view_y = border;
  float view_w = win_w - 2 * border;
  float view_h = win_h - 2 * border;

  /* minimal row height determined by font + padding */
  int font_height = TTF_GetFontHeight(g_font);
  float min_cell_h = (float)font_height + 2 * CELL_PADDING;
  float line_w = GRID_LINE_WIDTH;

  /* Compute column widths from cached max widths */
  sa.col_widths = malloc(g_cols * sizeof(int));
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++) {
    sa.col_widths[c] = g_max_col_widths[c] + 2 * CELL_PADDING;
    sa.total_grid_w += sa.col_widths[c];
  }
  sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);

  /* We'll iteratively compute need_horz/need_vert; row height may be adjusted
     after we know content area (which depends on scrollbars) so we iterate
     a few times to converge. */
  float row_h = min_cell_h;
  float total_grid_h =
      g_rows * row_h + (g_rows > 0 ? (g_rows - 1) * line_w : 0.0f);

  bool need_horz = false;
  bool need_vert = false;

  for (int iter = 0; iter < 6; iter++) {
    /* compute need_horz/need_vert using current total_grid_h */
    bool new_h = false;
    bool new_v = false;
    bool changed = true;
    float tmp_view_w = view_w;
    float tmp_view_h = view_h;
    /* iterative inside to account for interdependency between scrollbars */
    while (changed) {
      changed = false;
      float temp_w = tmp_view_w - (new_v ? SCROLLBAR_WIDTH : 0.0f);
      float temp_h = tmp_view_h - (new_h ? SCROLLBAR_WIDTH : 0.0f);
      bool calc_h = sa.total_grid_w > temp_w;
      bool calc_v = total_grid_h > temp_h;
      if (calc_h != new_h) {
        new_h = calc_h;
        changed = true;
      }
      if (calc_v != new_v) {
        new_v = calc_v;
        changed = true;
      }
    }

    need_horz = new_h;
    need_vert = new_v;

    /* compute content area with these flags */
    sa.content_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
    sa.content_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);

    /* If SNAP_VIEW_TO_ROWS, adjust row_h so that an integer number of rows fit
       exactly. We need to consider separator height line_w: content_h = N *
       cell_h + (N-1) * line_w
       => cell_h = (content_h + line_w)/N - line_w
       Choose N = floor((content_h + line_w) / (min_cell_h + line_w)) */
#if SNAP_VIEW_TO_ROWS
    if (sa.content_h > 0.0f) {
      float denom = min_cell_h + line_w;
      int N = (int)floorf((sa.content_h + line_w) / denom);
      if (N < 1)
        N = 1;
      float candidate = (sa.content_h + line_w) / (float)N - line_w;
      if (candidate < min_cell_h)
        candidate = min_cell_h;
      row_h = candidate;
    } else {
      row_h = min_cell_h;
    }
#else
    row_h = min_cell_h;
#endif

    /* recompute total_grid_h */
    float new_total_grid_h =
        g_rows * row_h + (g_rows > 0 ? (g_rows - 1) * line_w : 0.0f);
    if (fabsf(new_total_grid_h - total_grid_h) < 0.5f) {
      total_grid_h = new_total_grid_h;
      break;
    }
    total_grid_h = new_total_grid_h;
  }

  sa.need_horz = need_horz;
  sa.need_vert = need_vert;
  sa.total_grid_h = total_grid_h;
  sa.row_height = row_h;

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