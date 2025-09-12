#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Renderer *g_renderer = nullptr;
static SDL_Window *g_window = nullptr;
static bool g_window_fullscreened = true;

static int ROWS = 10;
static int COLS = 10;
constexpr float lineWidth = 1.0f; // Thickness of grid lines

constexpr SDL_Color BACKGROUND_COLOR = {240, 240, 240, 255}; // light gray
constexpr SDL_Color GRID_COLOR = {100, 100, 100, 255};       // dark gray

#define SDL_CHECK(call, msg, ret)                                              \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    ret;                                                                       \
  }

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

void draw(float w, float h) {
  // Draw background
  SDL_SetRenderDrawColor(g_renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g,
                         BACKGROUND_COLOR.b, BACKGROUND_COLOR.a);
  SDL_RenderClear(g_renderer);

  // Draw grid lines as rectangles
  SDL_SetRenderDrawColor(g_renderer, GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b,
                         GRID_COLOR.a);

  // Vertical lines
  for (int i = 0; i <= COLS; ++i) {
    float x = i * w / COLS;
    SDL_FRect rect = {
        .x = x - lineWidth / 2.0f, .y = 0.0f, .w = lineWidth, .h = h};
    SDL_RenderFillRect(g_renderer, &rect);
  }

  // Horizontal lines
  for (int i = 0; i <= ROWS; ++i) {
    float y = i * h / ROWS;
    SDL_FRect rect = {
        .x = 0.0f, .y = y - lineWidth / 2.0f, .w = w, .h = lineWidth};
    SDL_RenderFillRect(g_renderer, &rect);
  }

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    ROWS = atoi(argv[1]);
    COLS = atoi(argv[2]);
    if (ROWS <= 0 || COLS <= 0) {
      fprintf(stderr, "Columns and rows must be positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed", return 1);
  atexit(cleanup);

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_RESIZABLE, &g_window,
                                        &g_renderer),
            "Window and renderer creation failed", return 1);

  SDL_CHECK(SDL_SetWindowFullscreen(g_window, g_window_fullscreened),
            "Failed to set fullscreen mode", );

  bool running = true;
  SDL_Event event;

  int w, h;
  SDL_GetWindowSize(g_window, &w, &h);
  draw(w, h);

  while (running) {
    while (SDL_PollEvent(&event)) {
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
      case SDL_EVENT_WINDOW_RESIZED:
        SDL_GetWindowSize(g_window, &w, &h);
        draw(w, h);
        break;
      }
    }
  }

  return 0;
}