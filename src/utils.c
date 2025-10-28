#include "include/utils.h"
#include "include/config.h"
#include "include/globals.h"
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) ==
         0;
}

/* Unified cell update with width calculation.
 * This function updates both the cell text AND updates g_max_col_widths
 * if needed. This ensures headers, data rows, and dynamically updated cells
 * all contribute to column width calculations. */
void set_cell_with_width_update(int row, int col, const char *text) {
  if (row < 0 || row >= g_rows || col < 0 || col >= g_cols)
    return;

  free(g_grid[row][col].text);
  g_grid[row][col].text = strdup(text ? text : "");
  if (!g_grid[row][col].text) {
    g_grid[row][col].text_width = 0;
    g_grid[row][col].text_height = 0;
    return;
  }

  size_t len = strlen(g_grid[row][col].text);
  bool success = TTF_GetStringSize(g_font, g_grid[row][col].text, len,
                                   &g_grid[row][col].text_width,
                                   &g_grid[row][col].text_height);
  if (!success) {
    g_grid[row][col].text_width = 0;
    g_grid[row][col].text_height = 0;
    return;
  }

  /* Update max column width if this cell's text is wider */
  if (g_max_col_widths && col < g_cols) {
    g_max_col_widths[col] =
        SDL_max(g_max_col_widths[col], g_grid[row][col].text_width);
  }
}

/* Legacy function for backwards compatibility.
 * Now delegates to set_cell_with_width_update. */
void set_cell(int row, int col, const char *text) {
  set_cell_with_width_update(row, col, text);
}

int init_fs_log(void) {
  const char *path = ERROR_LOG_PATH;
  const char *mode = ERROR_LOG_MODE;
  FILE *f = fopen(path, mode);
  if (f) {
    g_log_file = f;
    return 0;
  }

  g_log_file = stderr;
  if (strcmp(ERROR_LOG_FALLBACK, "/dev/stderr") != 0 &&
      strcmp(ERROR_LOG_FALLBACK, "/dev/stdout") != 0) {
    FILE *f2 = fopen(ERROR_LOG_FALLBACK, "a");
    if (f2) {
      g_log_file = f2;
    }
  }
  fprintf(g_log_file,
          "Failed to open error log '%s' with mode '%s': %s\nUsing fallback "
          "'%s'.\n",
          path, mode, strerror(errno), ERROR_LOG_FALLBACK);
  fflush(g_log_file);
  return 1;
}

void close_fs_log(void) {
  if (!g_log_file)
    return;
  if (g_log_file != stderr && g_log_file != stdout) {
    fclose(g_log_file);
  }
  g_log_file = NULL;
}

void log_fs_error(const char *fmt, ...) {
  if (!g_log_file) {
    init_fs_log();
    if (!g_log_file)
      g_log_file = stderr;
  }

  va_list ap;
  va_start(ap, fmt);
  vfprintf(g_log_file, fmt, ap);
  fprintf(g_log_file, "\n");
  va_end(ap);
  fflush(g_log_file);
}