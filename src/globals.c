#include "include/globals.h"
#include "include/config.h"
#include "include/types.h"
#include "include/utils.h"
#include "include/virtual_scroll.h"
#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

SDL_Renderer *g_renderer = NULL;
SDL_Window *g_window = NULL;
TTF_Font *g_font = NULL;
Cell **g_grid = NULL;
int g_rows = DEFAULT_ROWS;
int g_cols = DEFAULT_COLS;
float g_offset_x = 0.0f;
float g_offset_y = 0.0f;
bool g_dragging_vert = false;
bool g_dragging_horz = false;
float g_drag_start_pos = 0.0f;
float g_drag_start_offset = 0.0f;
bool g_need_horz = false;
bool g_need_vert = false;
float g_total_grid_w = 0.0f;
float g_total_grid_h = 0.0f;
float g_content_w = 0.0f;
float g_content_h = 0.0f;
float g_view_x = 0.0f;
float g_view_y = 0.0f;
float g_view_w = 0.0f;
float g_view_h = 0.0f;

FILE *g_log_file = NULL;

float g_scroll_target_x = 0.0f;
float g_scroll_target_y = 0.0f;

SDL_Mutex *g_grid_mutex = NULL;

int *g_max_col_widths = NULL;

bool g_fs_traversing = false;

volatile bool g_stop = false;

int g_selected_row = -1;
int g_selected_col = -1;
int g_selected_index = -1;

float g_row_height = 0.0f;
float *g_col_left = NULL;
int *g_col_widths = NULL;

VirtualScrollState *g_vscroll = NULL;
float g_last_content_h = 0.0f;

void cleanup(void) {
  if (g_vscroll) {
    vscroll_cleanup(g_vscroll);
    g_vscroll = NULL;
  }
  if (g_grid_mutex) {
    SDL_DestroyMutex(g_grid_mutex);
    g_grid_mutex = NULL;
  }
  if (g_max_col_widths) {
    free(g_max_col_widths);
    g_max_col_widths = NULL;
  }
  if (g_col_left) {
    free(g_col_left);
    g_col_left = NULL;
  }
  if (g_col_widths) {
    free(g_col_widths);
    g_col_widths = NULL;
  }
  if (g_grid) {
    for (int r = 0; r < g_rows; r++) {
      if (g_grid[r]) {
        for (int c = 0; c < g_cols; c++) {
          if (g_grid[r][c].text) {
            free(g_grid[r][c].text);
          }
        }
        free(g_grid[r]);
      }
    }
    free(g_grid);
    g_grid = NULL;
  }
  if (g_font) {
    TTF_CloseFont(g_font);
    g_font = NULL;
  }
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = NULL;
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
    g_window = NULL;
  }
  if (g_log_file) {
    close_fs_log();
    g_log_file = NULL;
  }
  TTF_Quit();
  SDL_Quit();
}
