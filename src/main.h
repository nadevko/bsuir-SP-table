#ifndef IS_LOADED_SRC_MAIN_H
#define IS_LOADED_SRC_MAIN_H
#include <SDL3/SDL.h>

// --- CONFIG ---

#define ENABLE_VSYNC

#define WITH_BORDER

#define BORDER_COLOUR (SDL_Color){100, 100, 100, 255}
#define BORDER_WIDTH 1

#define WITH_GRID

#define DEFAULT_ROWS 10
#define DEFAULT_COLS 10

#define BACKGROUND_COLOUR (SDL_Color){240, 240, 240, 255}
#define GRID_COLOUR (SDL_Color){100, 100, 100, 255}
#define GRID_WIDTH 1.0

#define WITH_COLUMN_TITLE

#define COLUMN_TITLE_TEXT ""
#define COLUMN_TITLE_COLOUR (SDL_Color){100, 100, 100, 255}

// --- END CONFIG ---

#define SDL_CHECK(call, msg, ret)                                              \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    ret;                                                                       \
  }

static void cleanup(void);
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);
void draw();
int main(int argc, char *argv[]);

#endif
