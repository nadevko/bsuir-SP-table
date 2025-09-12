#include "main.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Renderer *g_renderer = nullptr;
static SDL_Window *g_window = nullptr;
static bool g_window_fullscreened = true;

// constexpr int lineWidth = 8;
// constexpr float margin = 0.0f;

// constexpr SDL_Color GRID_COLOR = {100, 100, 100, 255};
// constexpr SDL_Color OUTER_COLOR = {255, 255, 255, 255};

// constexpr

#define WITH_BORDER
#define WITH_GRID

#define SDL_CHECK(call, msg, ret)                                              \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    ret;                                                                       \
  }

#ifdef WITH_GRID
#define WITH_HORIZONTAL_LINES
#define WITH_VERTICAL_LINES
#endif

static void cleanup(void) {
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

// void draw(float x1, float y1, float x2, float y2) {

//   SDL_SetRenderDrawColor(g_renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g,
//                          BACKGROUND_COLOR.b, BACKGROUND_COLOR.a);
//   SDL_FRect bg_rect = {.x = grid_x1,
//                        .y = grid_y1,
//                        .w = grid_x2 - grid_x1,
//                        .h = grid_y2 - grid_y1};
//   SDL_RenderFillRect(g_renderer, &bg_rect);

//   SDL_SetRenderDrawColor(g_renderer, GRID_COLOR.r, GRID_COLOR.g,
//   GRID_COLOR.b,
//                          GRID_COLOR.a);

// #ifdef DRAW_VERTICAL_LINES
//   for (int i = 1; i < COLS; ++i) {
//     float x = grid_x1 + i * (grid_x2 - grid_x1) / COLS;
//     float rect_x = x - lineWidth / 2.0f; // Center the line
//     float rect_w = lineWidth;
//     if (rect_x + rect_w > grid_x2) {
//       rect_w = grid_x2 - rect_x;
//     }
//     if (rect_x < grid_x1) {
//       rect_w -= (grid_x1 - rect_x);
//       rect_x = grid_x1;
//     }
//     if (rect_w > 0) {
//       SDL_FRect rect = {
//           .x = rect_x, .y = grid_y1, .w = rect_w, .h = grid_y2 - grid_y1};
//       SDL_RenderFillRect(g_renderer, &rect);
//     }
//   }
// #endif

// #ifdef DRAW_HORIZONTAL_LINES
//   for (int i = 1; i < ROWS; ++i) {
//     float y = grid_y1 + i * (grid_y2 - grid_y1) / ROWS;
//     float rect_y = y - lineWidth / 2.0f;
//     float rect_h = lineWidth;

//     if (rect_y + rect_h > grid_y2) {
//       rect_h = grid_y2 - rect_y;
//     }
//     if (rect_y < grid_y1) {
//       rect_h -= (grid_y1 - rect_y);
//       rect_y = grid_y1;
//     }
//     if (rect_h > 0) {
//       SDL_FRect rect = {
//           .x = grid_x1, .y = rect_y, .w = grid_x2 - grid_x1, .h = rect_h};
//       SDL_RenderFillRect(g_renderer, &rect);
//     }
//   }
// #endif

// }

bool SDL_SetRenderDrawColorSimple(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw(unsigned int rows, unsigned int cols) {
  int x1 = 0, y1 = 0, x2, y2;

  SDL_GetWindowSize(g_window, &x2, &y2);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColorSimple(g_renderer, BORDER_COLOR);
  SDL_RenderClear(g_renderer);

  x1 += BORDER_WIDTH;
  y1 += BORDER_WIDTH;
  x2 -= BORDER_WIDTH;
  y2 -= BORDER_WIDTH;

  SDL_SetRenderClipRect(g_renderer, &(SDL_Rect){x1, y1, x2 - x1, y2 - y1});
  SDL_SetRenderDrawColorSimple(g_renderer, BACKGROUND_COLOR);
  SDL_RenderFillRect(g_renderer, &(SDL_FRect){x1, y1, x2, y2});
#else
  SDL_SetRenderDrawColorSimple(g_renderer, BACKGROUND_COLOR);
  SDL_RenderClear(g_renderer);
#endif

#ifdef WITH_HORIZONTAL_LINES
#endif

#ifdef WITH_VERTICAL_LINES
#endif

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  auto rows = ROWS;
  auto cols = COLS;

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
  atexit(cleanup);

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_RESIZABLE, &g_window,
                                        &g_renderer),
            "Window and renderer creation failed", return 1);

  SDL_CHECK(SDL_SetWindowFullscreen(g_window, g_window_fullscreened),
            "Failed to set fullscreen mode", g_window_fullscreened = false);

  bool running = true;
  SDL_Event event;

  while (running) {
    draw(rows, cols);
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