#include "main.h"
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
  /* The original code used TTF_GetStringSize; keep the original API call names
     since user's environment apparently used them. If compilation complains,
     replace with appropriate SDL_ttf function (e.g., TTF_SizeUTF8). */
  bool success = TTF_GetStringSize(g_font, g_grid[row][col].text, len,
                                   &g_grid[row][col].text_width,
                                   &g_grid[row][col].text_height);
  if (!success) {
    g_grid[row][col].text_width = 0;
    g_grid[row][col].text_height = 0;
  }
}
