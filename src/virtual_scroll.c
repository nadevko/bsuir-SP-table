#include "include/virtual_scroll.h"
#include "include/config.h"
#include "include/globals.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

VirtualScrollState *vscroll_init(int cols) {
  VirtualScrollState *vs = malloc(sizeof(VirtualScrollState));
  if (!vs)
    return NULL;

  vs->buffer = malloc(VSCROLL_BUFFER_SIZE * sizeof(Cell *));
  if (!vs->buffer) {
    free(vs);
    return NULL;
  }

  for (int i = 0; i < VSCROLL_BUFFER_SIZE; i++) {
    vs->buffer[i] = calloc(cols, sizeof(Cell));
    if (!vs->buffer[i]) {
      for (int j = 0; j < i; j++)
        free(vs->buffer[j]);
      free(vs->buffer);
      free(vs);
      return NULL;
    }
  }

  vs->texture_cache =
      malloc(VSCROLL_BUFFER_SIZE * cols * sizeof(SDL_Texture *));
  vs->texture_valid = malloc(VSCROLL_BUFFER_SIZE * cols * sizeof(bool));

  if (!vs->texture_cache || !vs->texture_valid) {
    free(vs->texture_cache);
    free(vs->texture_valid);
    for (int i = 0; i < VSCROLL_BUFFER_SIZE; i++)
      free(vs->buffer[i]);
    free(vs->buffer);
    free(vs);
    return NULL;
  }

  memset(vs->texture_cache, 0,
         VSCROLL_BUFFER_SIZE * cols * sizeof(SDL_Texture *));
  memset(vs->texture_valid, 0, VSCROLL_BUFFER_SIZE * cols * sizeof(bool));

  vs->buffer_start_row = 0;
  vs->buffer_count = 0;
  vs->desired_start_row = 0;
  vs->needs_reload = true;
  vs->total_virtual_rows = 0;

  return vs;
}

void vscroll_cleanup(VirtualScrollState *vs) {
  if (!vs)
    return;

  vscroll_invalidate_all_textures(vs);

  if (vs->buffer) {
    for (int i = 0; i < VSCROLL_BUFFER_SIZE; i++) {
      if (vs->buffer[i]) {
        for (int c = 0; c < g_cols; c++) {
          if (vs->buffer[i][c].text) {
            free(vs->buffer[i][c].text);
            vs->buffer[i][c].text = NULL;
          }
        }
        free(vs->buffer[i]);
      }
    }
    free(vs->buffer);
  }

  free(vs->texture_cache);
  free(vs->texture_valid);
  free(vs);
}

void vscroll_update_buffer_position(VirtualScrollState *vs, float view_y,
                                    float content_h, float row_height) {
  if (!vs)
    return;

  float line_w = GRID_LINE_WIDTH;
  float row_full = row_height + line_w;

  int first_visible_row = (int)floorf(g_offset_y / row_full);
  if (first_visible_row < 0)
    first_visible_row = 0;
  /* ЗАЩИТА: не можем начинать дальше последней строки */
  if (first_visible_row >= vs->total_virtual_rows)
    first_visible_row =
        vs->total_virtual_rows > 0 ? vs->total_virtual_rows - 1 : 0;

  int visible_count = (int)ceilf(content_h / row_full) + 1;
  int prefetch_before = VSCROLL_PREFETCH;
  int prefetch_after = VSCROLL_PREFETCH;

  int desired_start = first_visible_row - prefetch_before;
  int desired_end = first_visible_row + visible_count + prefetch_after;

  if (desired_start < 0)
    desired_start = 0;
  if (desired_end > vs->total_virtual_rows)
    desired_end = vs->total_virtual_rows;

  vs->desired_start_row = desired_start;

  int desired_count = desired_end - desired_start;

  if (vs->desired_start_row < vs->buffer_start_row ||
      vs->desired_start_row + desired_count >
          vs->buffer_start_row + vs->buffer_count ||
      desired_count > VSCROLL_BUFFER_SIZE) {
    vs->needs_reload = true;
  }
}

void vscroll_load_from_grid(VirtualScrollState *vs, int start_row, int count) {
  if (!vs)
    return;
  if (count > VSCROLL_BUFFER_SIZE)
    count = VSCROLL_BUFFER_SIZE;
  if (start_row < 0)
    start_row = 0;
  if (start_row + count > vs->total_virtual_rows)
    count = vs->total_virtual_rows - start_row;

  SDL_LockMutex(g_grid_mutex);

  vscroll_invalidate_all_textures(vs);

  for (int i = 0; i < VSCROLL_BUFFER_SIZE; i++) {
    for (int c = 0; c < g_cols; c++) {
      if (vs->buffer[i][c].text) {
        free(vs->buffer[i][c].text);
        vs->buffer[i][c].text = NULL;
      }
    }
  }

  for (int i = 0; i < count; i++) {
    int src_row = start_row + i;
    if (src_row < 0 || src_row >= g_rows)
      continue;

    int buf_idx = i;
    for (int c = 0; c < g_cols; c++) {
      if (g_grid[src_row] && g_grid[src_row][c].text) {
        vs->buffer[buf_idx][c].text =
            malloc(strlen(g_grid[src_row][c].text) + 1);
        if (vs->buffer[buf_idx][c].text) {
          strcpy(vs->buffer[buf_idx][c].text, g_grid[src_row][c].text);
        }
      }
      if (g_grid[src_row]) {
        vs->buffer[buf_idx][c].text_width = g_grid[src_row][c].text_width;
        vs->buffer[buf_idx][c].text_height = g_grid[src_row][c].text_height;
      }
    }
  }

  vs->buffer_start_row = start_row;
  vs->buffer_count = count;
  vs->needs_reload = false;

  SDL_UnlockMutex(g_grid_mutex);
}

Cell *vscroll_get_cell(VirtualScrollState *vs, int virtual_row, int col) {
  if (!vs || col < 0 || col >= g_cols)
    return NULL;

  int buf_idx = virtual_row - vs->buffer_start_row;
  if (buf_idx < 0 || buf_idx >= vs->buffer_count)
    return NULL;

  return &vs->buffer[buf_idx][col];
}

SDL_Texture *vscroll_get_cached_texture(VirtualScrollState *vs, int buf_idx,
                                        int col) {
  if (!vs || buf_idx < 0 || buf_idx >= VSCROLL_BUFFER_SIZE || col < 0 ||
      col >= g_cols)
    return NULL;

  int idx = buf_idx * g_cols + col;
  if (!vs->texture_valid[idx])
    return NULL;

  return vs->texture_cache[idx];
}

void vscroll_set_cached_texture(VirtualScrollState *vs, int buf_idx, int col,
                                SDL_Texture *tex) {
  if (!vs || buf_idx < 0 || buf_idx >= VSCROLL_BUFFER_SIZE || col < 0 ||
      col >= g_cols)
    return;

  int idx = buf_idx * g_cols + col;
  vs->texture_cache[idx] = tex;
  vs->texture_valid[idx] = (tex != NULL);
}

void vscroll_invalidate_all_textures(VirtualScrollState *vs) {
  if (!vs)
    return;

  for (int i = 0; i < VSCROLL_BUFFER_SIZE * g_cols; i++) {
    if (vs->texture_cache[i]) {
      SDL_DestroyTexture(vs->texture_cache[i]);
      vs->texture_cache[i] = NULL;
    }
    vs->texture_valid[i] = false;
  }
}