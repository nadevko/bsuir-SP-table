#include "main.h"
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>

static void cleanup(void) {
  if (g_font != nullptr) {
    TTF_CloseFont(g_font);
    g_font = nullptr;
  }
  if (g_renderer != nullptr) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window != nullptr) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
  SDL_Quit();
}

bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw() {
  int x = 0, y = 0, w, h;

  SDL_GetWindowSize(g_window, &w, &h);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);

  x += BORDER_WIDTH;
  y += BORDER_WIDTH;
  w -= x + BORDER_WIDTH;
  h -= y + BORDER_WIDTH;

  SDL_SetRenderClipRect(g_renderer, &(SDL_Rect){x, y, w, h});
  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer, &(SDL_FRect){x, y, w, h});
#else
  SDL_SetRenderDrawColorSimple(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

#ifdef WITH_GRID
  int count = rows + cols;
  SDL_FRect rects[count];
  int idx = 0;

  float cell_h = (float)h / rows;
  SDL_FRect grid_line_row = {x, y - GRID_LINE_HALF_WIDTH, w, GRID_LINE_WIDTH};

  for (int i = 1; i < rows; i++) {
    grid_line_row.y += cell_h;
    rects[idx++] = grid_line_row;
  }

  float cell_w = (float)w / cols;
  SDL_FRect grid_line_col = {x - GRID_LINE_HALF_WIDTH, y, GRID_LINE_WIDTH, h};

  for (int i = 1; i < cols; i++) {
    grid_line_col.x += cell_w;
    rects[idx++] = grid_line_col;
  }

  SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
  SDL_RenderFillRects(g_renderer, rects, count);
#endif

#ifdef WITH_COLUMN_TITLE
  float cell_x = x;

  for (int col = 0; col < cols; col++) {
    cell_x += cell_w;
    // TTF_RenderText_Solid(TTF_Font *font, const char *text, size_t length, SDL_Color fg);
  }
#endif

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    rows = atoi(argv[1]);
    cols = atoi(argv[2]);
    if (rows <= 0 || cols <= 0) {
      fprintf(stderr, "Columns and rows must be positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialization failed", return 1);
  SDL_CHECK(TTF_Init(), "SDL-ttf initialization failed", return 1);
  atexit(cleanup);

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed", return 1);

  bool running = true;
  SDL_Event event;

  while (running) {
    draw();
    SDL_WaitEvent(&event);

    switch (event.type) {
    case SDL_EVENT_QUIT:
      running = false;
      break;
    case SDL_EVENT_KEY_DOWN:
      switch (event.key.key) {
      case SDLK_ESCAPE:
        running = false;
        break;
      }
      break;
    }
  }

  return 0;
}