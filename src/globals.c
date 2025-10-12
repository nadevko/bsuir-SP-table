#include "main.h"
#include <stdio.h>
#include <stdlib.h>

/* Definitions of globals (previously 'static' in the big file) */
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

/* Log file pointer (initialized by init_fs_log) */
FILE *g_log_file = NULL;

/* Smooth scroll targets (mouse wheel / touchpad) */
float g_scroll_target_x = 0.0f;
float g_scroll_target_y = 0.0f;

/* Mutex (initialized in main) */
SDL_Mutex *g_grid_mutex = NULL;

/* Max column widths (initialized in main) */
int *g_max_col_widths = NULL;

/* FS traversing flag */
bool g_fs_traversing = false;

/* Stop flag */
volatile bool g_stop = false;

void cleanup(void) {
  if (g_grid_mutex) {
    SDL_DestroyMutex(g_grid_mutex);
    g_grid_mutex = NULL;
  }

  if (g_max_col_widths) {
    free(g_max_col_widths);
    g_max_col_widths = NULL;
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