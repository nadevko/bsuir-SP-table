#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
  char *text;
  int text_width;
  int text_height;
} Cell;

typedef struct SizeAlloc {
  bool need_horz;
  bool need_vert;
  float total_grid_w;
  float total_grid_h;
  float content_w;
  float content_h;
  int *col_widths;
  float *col_left;
  float row_height;
} SizeAlloc;
