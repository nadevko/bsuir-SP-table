#include "main.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Clean-up helper */
void cleanup(void) {
  if (g_font != NULL) {
    TTF_CloseFont(g_font);
    g_font = NULL;
  }

  if (g_grid != NULL) {
    for (int r = 0; r < g_rows; r++) {
      for (int c = 0; c < g_cols; c++) {
        free(g_grid[r][c].text);
      }
      free(g_grid[r]);
    }
    free(g_grid);
    g_grid = NULL;
  }

  if (g_renderer != NULL) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = NULL;
  }
  if (g_window != NULL) {
    SDL_DestroyWindow(g_window);
    g_window = NULL;
  }

  /* close log file if it's not stderr */
  close_fs_log();

  SDL_Quit();
}

/* Utility wrapper (keeps your naming) */
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) ==
         0;
}

/* Set a cell (update string + measured size). */
void set_cell(int row, int col, const char *text) {
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
  /* If TTF_GetStringSize isn't available on target, user may replace with
     TTF_SizeUTF8 or similar — we preserve original call names to minimize
     changes. */
  bool success = TTF_GetStringSize(g_font, g_grid[row][col].text, len,
                                   &g_grid[row][col].text_width,
                                   &g_grid[row][col].text_height);
  if (!success) {
    g_grid[row][col].text_width = 0;
    g_grid[row][col].text_height = 0;
  }
}

/* Logging helpers implementation */

/* Try to open ERROR_LOG_PATH according to ERROR_LOG_APPEND.
 * If cannot open, fallback to ERROR_LOG_FALLBACK (usually /dev/stderr) and
 * write first error message there notifying about fallback.
 *
 * Returns 0 if opened requested file, 1 if fallback used.
 */
int init_fs_log(void) {
  const char *path = ERROR_LOG_PATH;
  const char *mode = (ERROR_LOG_APPEND ? "a" : "w");
  FILE *f = fopen(path, mode);
  if (f) {
    g_log_file = f;
    return 0;
  }

  /* failed to open requested file: fallback to ERROR_LOG_FALLBACK */
  g_log_file = stderr; /* by default point to stderr */
  /* try to open fallback as FILE* if it's not standard stderr/stdout */
  if (strcmp(ERROR_LOG_FALLBACK, "/dev/stderr") != 0 &&
      strcmp(ERROR_LOG_FALLBACK, "/dev/stdout") != 0) {
    FILE *f2 = fopen(ERROR_LOG_FALLBACK, "a");
    if (f2) {
      g_log_file = f2;
    } else {
      /* keep stderr; but notify what happened */
    }
  }
  /* write immediate message to selected fallback (stderr or opened fallback) */
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
  /* if g_log_file points to a stream different from stderr/stdout, close it */
  if (g_log_file != stderr && g_log_file != stdout) {
    fclose(g_log_file);
  }
  g_log_file = NULL;
}

void log_fs_error(const char *fmt, ...) {
  if (!g_log_file) {
    /* try to initialize (best-effort) */
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
