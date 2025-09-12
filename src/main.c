#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Renderer *g_renderer = nullptr;
static SDL_Window *g_window = nullptr;
static bool g_window_fullscreened = true;

static int ROWS = 10;
static int COLS = 10;

constexpr SDL_Color BACKGROUND_COLOR = {240, 240, 240, 255}; // light gray
constexpr SDL_Color GRID_COLOR = {100, 100, 100, 255};       // dark gray
// constexpr

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
  // Draw backgroud
  SDL_SetRenderDrawColor(g_renderer, 240, 240, 240, 255);
  SDL_RenderClear(g_renderer);

  // Draw lines
  SDL_SetRenderDrawColor(g_renderer, 100, 100, 100, 255);

  for (int i = 0; i <= COLS; ++i) {
    float x = (float)i * w / COLS;
    SDL_RenderLine(g_renderer, x, 0.0f, x, h);
  }

  for (int i = 0; i <= ROWS; ++i) {
    float y = (float)i * h / ROWS;
    SDL_RenderLine(g_renderer, 0.0f, y, w, y);
  }

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    ROWS = atoi(argv[1]);
    COLS = atoi(argv[2]);
    if (ROWS <= 0 || COLS <= 0) {
      fprintf(stderr, "Columns and rows must be a positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed", return 1)
  atexit(cleanup);

  SDL_CHECK(SDL_CreateWindowAndRenderer("", 300, 480, SDL_WINDOW_RESIZABLE,
                                        &g_window, &g_renderer),
            "Window and renderer creation are failed", return 1)

  SDL_CHECK(SDL_SetWindowFullscreen(g_window, true),
            "Warning: Failed to set fullscreen mode", )

  bool running = true;
  SDL_Event event;

  int w, h;
  SDL_GetWindowSize(g_window, &w, &h);
  draw(w, h);

  while (running)
    while (SDL_PollEvent(&event))
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