#pragma once
#include "types.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

#define VSCROLL_BUFFER_SIZE 500
#define VSCROLL_PREFETCH 100

typedef struct {
  int buffer_start_row;
  int buffer_count;
  Cell **buffer;
  int desired_start_row;
  bool needs_reload;
  int total_virtual_rows;

  SDL_Texture **texture_cache;
  bool *texture_valid;
} VirtualScrollState;

VirtualScrollState *vscroll_init(int cols);
void vscroll_cleanup(VirtualScrollState *vs);
void vscroll_update_buffer_position(VirtualScrollState *vs, float view_y,
                                    float content_h, float row_height);
void vscroll_load_from_grid(VirtualScrollState *vs, int start_row, int count);
Cell *vscroll_get_cell(VirtualScrollState *vs, int virtual_row, int col);
SDL_Texture *vscroll_get_cached_texture(VirtualScrollState *vs, int buf_idx,
                                        int col);
void vscroll_set_cached_texture(VirtualScrollState *vs, int buf_idx, int col,
                                SDL_Texture *tex);
void vscroll_invalidate_all_textures(VirtualScrollState *vs);
