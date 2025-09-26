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
#define DEFAULT_COLS 4 // File, Size (bytes), Date, Permissions

#define GRID_BACKGROUND_COLOUR (SDL_Color){240, 240, 240, 255}
#define GRID_LINE_COLOUR BORDER_COLOUR
#define GRID_LINE_WIDTH (float)BORDER_WIDTH

#define WITH_FONTCONFIG

#define LEFT 0
#define CENTER 1
#define RIGHT 2
#define TOP 0
#define BOTTOM 2

#define CELL_TEXT_NAME "Ubuntu Mono"
#define CELL_TEXT_SIZE 32.0
#define CELL_TEXT_POSITION_HORIZONTAL CENTER // LEFT || CENTER || RIGHT
#define CELL_TEXT_POSITION_VERTICAL BOTTOM   // TOP || CENTER || BOTTOM
#define CELL_TEXT_COLOUR (SDL_Color){100, 100, 100, 255}

#define CELL_PADDING 10
#define SCROLLBAR_WIDTH 20
#define SCROLL_SPEED 50

#define SCROLLBAR_BG_COLOUR (SDL_Color){200, 200, 200, 255}
#define SCROLLBAR_THUMB_COLOUR (SDL_Color){100, 100, 100, 255}

#define NATURAL_SCROLL 0

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

typedef struct {
  char *text;
  int text_width;
  int text_height;
} Cell;

static void cleanup(void);
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);
void draw(void);
void set_cell(int row, int col, const char *text);
int main(int argc, char *argv[]);

static SDL_Renderer *g_renderer = NULL;
static SDL_Window *g_window = NULL;
static TTF_Font *g_font = NULL;
static Cell **g_grid = NULL;

static int g_rows = DEFAULT_ROWS;
static int g_cols = DEFAULT_COLS;

static float g_offset_x = 0.0f;
static float g_offset_y = 0.0f;

static bool g_dragging_vert = false;
static bool g_dragging_horz = false;
static float g_drag_start_pos = 0.0f;
static float g_drag_start_offset = 0.0f;

static bool g_need_horz = false;
static bool g_need_vert = false;
static float g_total_grid_w = 0.0f;
static float g_total_grid_h = 0.0f;
static float g_content_w = 0.0f;
static float g_content_h = 0.0f;
static float g_view_x = 0.0f;
static float g_view_y = 0.0f;
static float g_view_w = 0.0f;
static float g_view_h = 0.0f;

typedef struct SizeAlloc {
  bool need_horz;
  bool need_vert;
  float total_grid_w;
  float total_grid_h;
  float content_w;
  float content_h;
  int *col_widths; // malloc'ed array of length g_cols
  float *col_left; // malloc'ed array of length g_cols
} SizeAlloc;

#endif