#ifndef IS_LOADED_SRC_MAIN_H
#define IS_LOADED_SRC_MAIN_H
#include <SDL3/SDL.h>

// --- CONFIG ---

#define WITH_BORDER
#define WITH_GRID

#define ROWS 10
#define COLS 10

#define BACKGROUND_COLOR (SDL_Color){240, 240, 240, 255}
#define BORDER_COLOR (SDL_Color){100, 100, 100, 255}
#define BORDER_WIDTH 200

// --- CONFIG ---

#define SDL_CHECK(call, msg, ret)                                              \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    ret;                                                                       \
  }

static void cleanup(void);
bool SDL_SetRenderDrawColorSimple(SDL_Renderer *renderer, SDL_Color color);
void draw(unsigned int rows, unsigned int cols);
int main(int argc, char *argv[]);

#endif
