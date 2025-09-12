#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  // default table sizing
  int rows = 10;
  int cols = 10;

  if (argc == 3) {
    rows = atoi(argv[1]);
    cols = atoi(argv[2]);
    if (rows <= 0 || cols <= 0) {
      fprintf(stderr, "Columns and rows must be positive.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL initialisation failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  if (!SDL_CreateWindowAndRenderer("", 300, 480, SDL_WINDOW_RESIZABLE, &window,
                                   &renderer)) {
    fprintf(stderr, "Window and renderer creation are failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  if (!SDL_SetWindowFullscreen(window, true)) {
    fprintf(stderr,
            "Warning: Failed to set fullscreen mode: %s\n",
            SDL_GetError());
  }

  bool running = true;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE) {
        running = false;
      }
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Draw backgroud
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);

    // Draw lines
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);

    for (int i = 0; i <= cols; ++i) {
      float x = (float)i * w / cols;
      SDL_RenderLine(renderer, x, 0.0f, x, (float)h);
    }

    for (int i = 0; i <= rows; ++i) {
      float y = (float)i * h / rows;
      SDL_RenderLine(renderer, 0.0f, y, (float)w, y);
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}