#ifndef _IS_LOADED__MAIN_H_
#define _IS_LOADED__MAIN_H_

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>

/* --- CONFIG --- */

#define WITH_BORDER

#define BORDER_COLOUR (SDL_Color){100, 100, 100, 255}
#define BORDER_WIDTH 2

#define WITH_GRID

#define DEFAULT_ROWS 20
#define DEFAULT_COLS 4

#define GRID_BACKGROUND_COLOUR (SDL_Color){240, 240, 240, 255}
#define GRID_LINE_COLOUR BORDER_COLOUR
#define GRID_LINE_WIDTH (float)BORDER_WIDTH

#define WITH_FONTCONFIG

#define RECURSIVE_LISTING

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

/* --- CONFIG --- */

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

typedef struct SizeAlloc {
  bool need_horz;
  bool need_vert;
  float total_grid_w;
  float total_grid_h;
  float content_w;
  float content_h;
  int *col_widths;
  float *col_left;
} SizeAlloc;

/* Globals (defined in src/globals.c) */
extern SDL_Renderer *g_renderer;
extern SDL_Window *g_window;
extern TTF_Font *g_font;
extern Cell **g_grid;

extern int g_rows;
extern int g_cols;

extern float g_offset_x;
extern float g_offset_y;

extern bool g_dragging_vert;
extern bool g_dragging_horz;
extern float g_drag_start_pos;
extern float g_drag_start_offset;

extern bool g_need_horz;
extern bool g_need_vert;
extern float g_total_grid_w;
extern float g_total_grid_h;
extern float g_content_w;
extern float g_content_h;
extern float g_view_x;
extern float g_view_y;
extern float g_view_w;
extern float g_view_h;

/* Prototypes */
void cleanup(void);
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);
void set_cell(int row, int col, const char *text);
SizeAlloc sizeAllocate(int win_w, int win_h);
void draw_with_alloc(const SizeAlloc *sa);

/* File system helpers */
int count_files(const char *dir_path);
void populate_files(const char *dir_path, int start_row);

/* main */
int main(int argc, char *argv[]);

#endif
