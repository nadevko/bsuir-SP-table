#pragma once

#include <stdarg.h>
#include <stdbool.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define ANY_CHECK(call, msg)                                                   \
  do {                                                                         \
    if (!(call)) {                                                             \
      fprintf(stderr, "%s\n", msg);                                            \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define SDL_CHECK(call, msg)                                                   \
  do {                                                                         \
    if (!(call)) {                                                             \
      fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                        \
      return 1;                                                                \
    }                                                                          \
  } while (0)

bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);

/* Unified cell update: sets text AND updates max column width.
 * Use this for all cell modifications to ensure consistent width calculations.
 */
void set_cell_with_width_update(int row, int col, const char *text);

/* Legacy wrapper for backwards compatibility.
 * Calls set_cell_with_width_update internally. */
void set_cell(int row, int col, const char *text);

int init_fs_log(void);
void close_fs_log(void);
void log_fs_error(const char *fmt, ...);