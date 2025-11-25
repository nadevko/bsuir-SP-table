/* src/layout.c */
#include "include/layout.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/table_model.h"
#include "include/types.h"
#include <math.h>
#include <stdlib.h>

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

  int font_height = TTF_GetFontHeight(g_font);
  float min_cell_h = (float)font_height + 2 * CELL_PADDING;
  float line_w = GRID_LINE_WIDTH;

  /* Get column count from table */
  int col_count = g_table ? table_get_col_count(g_table) : g_cols;

  if (col_count == 0) {
    sa.need_horz = false;
    sa.need_vert = false;
    sa.total_grid_w = 0.0f;
    sa.total_grid_h = 0.0f;
    sa.content_w = view_w;
    sa.content_h = view_h;
    sa.row_height = min_cell_h;
    sa.col_widths = NULL;
    sa.col_left = NULL;
    return sa;
  }

  /* Step 1: Calculate base widths from content */
  sa.col_widths = malloc((size_t)col_count * sizeof *sa.col_widths);
  if (!sa.col_widths) {
    sa.total_grid_w = 0.0f;
    sa.total_grid_h = 0.0f;
    sa.content_w = view_w;
    sa.content_h = view_h;
    sa.row_height = min_cell_h;
    sa.col_left = NULL;
    return sa;
  }

  if (g_table) {
    for (int c = 0; c < col_count; c++) {
      sa.col_widths[c] = table_get_col_width(g_table, c);
    }
  } else {
    /* Fallback to g_max_col_widths */
    for (int c = 0; c < col_count; c++) {
      sa.col_widths[c] = (g_max_col_widths && c < g_cols)
                             ? g_max_col_widths[c] + 2 * CELL_PADDING
                             : 100;
    }
  }

  /* Calculate total width with grid lines */
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < col_count; c++) {
    sa.total_grid_w += (float)sa.col_widths[c];
  }
  /* Add grid lines between columns (only if more than 1 column) */
  if (col_count > 1) {
    sa.total_grid_w += (float)(col_count - 1) * line_w;
  }

  /* Calculate row height and grid height */
  float row_h = min_cell_h;
  float total_grid_h = 0.0f;

  int row_count = g_table ? table_get_row_count(g_table) : g_rows;
  if (row_count > 0) {
    /* Header row + data rows */
    int total_rows = row_count + 1;
    total_grid_h = (float)total_rows * row_h;
    /* Add grid lines between rows */
    if (total_rows > 1) {
      total_grid_h += (float)(total_rows - 1) * line_w;
    }
  }

  sa.row_height = row_h;

  /* Detect if we need scrollbars - initial pass */
  bool need_horz = sa.total_grid_w > view_w;
  bool need_vert = total_grid_h > view_h;

  /* Account for scrollbar taking space */
  float available_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
  float available_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);

  /* Step 2: If content is smaller than available space, stretch columns */
  if (sa.total_grid_w < available_w && col_count > 0) {
    float extra_space = available_w - sa.total_grid_w;
    float per_column = extra_space / (float)col_count;

    for (int c = 0; c < col_count; c++) {
      sa.col_widths[c] = (int)((float)sa.col_widths[c] + per_column);
    }

    sa.total_grid_w = available_w;
  }

  /* Recalculate scrollbar needs with stretched columns */
  need_horz = sa.total_grid_w > available_w;
  need_vert = total_grid_h > available_h;

  sa.content_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
  sa.content_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);

  sa.need_horz = need_horz;
  sa.need_vert = need_vert;
  sa.total_grid_h = total_grid_h;

  /* Calculate column positions */
  sa.col_left = malloc((size_t)col_count * sizeof *sa.col_left);
  if (col_count > 0) {
    sa.col_left[0] = 0.0f;
  }
  for (int c = 1; c < col_count; c++) {
    sa.col_left[c] = sa.col_left[c - 1] + (float)sa.col_widths[c - 1];
    if (c > 0) {
      sa.col_left[c] += line_w; /* Add line width before this column */
    }
  }

  g_view_x = view_x;
  g_view_y = view_y;
  g_view_w = view_w;
  g_view_h = view_h;

  /* Detect resize for virtual scroll */
  if (fabsf(sa.content_h - g_last_content_h) > 1.0f) {
    if (g_vscroll) {
      g_vscroll->needs_reload = true;
    }
    g_last_content_h = sa.content_h;
  }

  return sa;
}