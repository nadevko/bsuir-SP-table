/* src/layout.c */
#include "include/layout.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/types.h"
#include <math.h>
#include <stdlib.h>

/* Флаг: если 1 — распределять оставшееся горизонтальное пространство
 * поровну между столбцами. Можно переопределить в config.h */
#ifndef EXPAND_COLUMNS_TO_FILL
#define EXPAND_COLUMNS_TO_FILL 1
#endif

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

  sa.col_widths = malloc(g_cols * sizeof(int));
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++) {
    sa.col_widths[c] = g_max_col_widths[c] + 2 * CELL_PADDING;
    sa.total_grid_w += sa.col_widths[c];
  }
  sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);

  float row_h = min_cell_h;
  /* ИСПРАВЛЕНИЕ: высота грида = N строк + (N-1) линий между ними
     Было: g_rows * row_h + (g_rows - 1) * line_w
     Это давало лишнее пространство */
  float total_grid_h = 0.0f;
  if (g_rows > 0) {
    total_grid_h = g_rows * row_h + (g_rows - 1) * line_w;
  }

  bool need_horz = false;
  bool need_vert = false;

  for (int iter = 0; iter < 6; iter++) {
    bool new_h = false;
    bool new_v = false;
    bool changed = true;
    float tmp_view_w = view_w;
    float tmp_view_h = view_h;
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

    sa.content_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
    sa.content_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);

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

    /* ИСПРАВЛЕНИЕ: правильный расчёт высоты грида
       Высота = сумма всех ячеек + линии МЕЖДУ ними (не после последней!)
       N строк = N * cell_h + (N-1) * line_w */
    float new_total_grid_h = 0.0f;
    if (g_rows > 0) {
      new_total_grid_h = g_rows * row_h + (g_rows - 1) * line_w;
    }

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

  /* Если включена опция — расширяем начальные ширины столбцов,
   * чтобы занять свободное горизонтальное пространство ровно. */
#if EXPAND_COLUMNS_TO_FILL
  if (g_cols > 0) {
    float line_w = GRID_LINE_WIDTH;
    /* текущее total_grid_w учитывает ширины + межколоночные линии */
    float current_total_w = 0.0f;
    for (int c = 0; c < g_cols; c++)
      current_total_w += sa.col_widths[c];
    current_total_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);

    /* content_w уже посчитан (с учётом полос прокрутки) */
    float extra_f = sa.content_w - current_total_w;
    if (extra_f > 1.0f) {
      /* распределяем целые пиксели */
      int extra = (int)floorf(extra_f);
      int add_each = extra / g_cols;
      int remainder = extra - add_each * g_cols;
      for (int c = 0; c < g_cols; c++) {
        sa.col_widths[c] += add_each;
      }
      for (int c = 0; c < remainder; c++) {
        sa.col_widths[c] += 1;
      }
      /* обновим total_grid_w */
      sa.total_grid_w = 0.0f;
      for (int c = 0; c < g_cols; c++)
        sa.total_grid_w += sa.col_widths[c];
      sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);
    } else {
      /* если не расширяли, сохранить sa.total_grid_w как было */
      sa.total_grid_w = current_total_w;
      sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);
    }
  } else {
    /* EXPAND_COLUMNS_TO_FILL выключен — восстановим total_grid_w как рассчитано
     * изначально */
    sa.total_grid_w = 0.0f;
    for (int c = 0; c < g_cols; c++)
      sa.total_grid_w += sa.col_widths[c];
    sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * GRID_LINE_WIDTH : 0.0f);
  }
#else
  /* без расширения — просто пересчитываем total_grid_w на всякий случай */
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++)
    sa.total_grid_w += sa.col_widths[c];
  sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * GRID_LINE_WIDTH : 0.0f);
#endif

  sa.col_left = malloc(g_cols * sizeof(float));
  if (g_cols > 0)
    sa.col_left[0] = 0.0f;
  for (int c = 1; c < g_cols; c++) {
    sa.col_left[c] = sa.col_left[c - 1] + sa.col_widths[c - 1] + line_w;
  }

  g_view_x = view_x;
  g_view_y = view_y;
  g_view_w = view_w;
  g_view_h = view_h;

  /* Детекция resize для virtual scroll */
  if (fabsf(sa.content_h - g_last_content_h) > 1.0f) {
    if (g_vscroll) {
      g_vscroll->needs_reload = true;
    }
    g_last_content_h = sa.content_h;
  }

  return sa;
}
