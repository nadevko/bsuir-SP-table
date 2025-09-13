#ifndef _IS_LOADED__MAIN_H_
#define _IS_LOADED__MAIN_H_
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>

// --- CONFIG ---

#define WITH_BORDER

#define BORDER_COLOUR (SDL_Color){100, 100, 100, 255}
#define BORDER_WIDTH 2

#define WITH_GRID

#define DEFAULT_ROWS 20
#define DEFAULT_COLS 10

#define GRID_BACKGROUND_COLOUR (SDL_Color){240, 240, 240, 255}
#define GRID_LINE_COLOUR BORDER_COLOUR
#define GRID_LINE_WIDTH (float)BORDER_WIDTH

#define WITH_COLUMN_TITLE

#define COLUMN_TITLE_TEXT_LABEL "Column %i"
#define COLUMN_TITLE_TEXT_NAME "Ubuntu Mono"
#define COLUMN_TITLE_TEXT_SIZE 32.0
#define COLUMN_TITLE_TEXT_POSITION_HORIZONTAL CENTER // LEFT || CENTER || RIGHT
#define COLUMN_TITLE_TEXT_POSITION_VERTICAL CENTER // TOP || CENTER || BOTTOM
#define COLUMN_TITLE_TEXT_COLOUR (SDL_Color){100, 100, 100, 255}

// --- END CONFIG ---

#define ANY_CHECK(call, msg)                                                   \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s\n", msg);                                              \
    return 1;                                                                  \
  }

#define SDL_CHECK(call, msg)                                                   \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    return 1;                                                                  \
  }

static void cleanup(void);
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);
void draw();
int main(int argc, char *argv[]);

static SDL_Renderer *g_renderer = nullptr;
static SDL_Window *g_window = nullptr;
static TTF_Font *g_font = nullptr;

static int rows = DEFAULT_ROWS;
static int cols = DEFAULT_COLS;

#endif
