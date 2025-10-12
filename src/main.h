#ifndef _IS_LOADED__MAIN_H_
#define _IS_LOADED__MAIN_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_thread.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>

/* --- CONFIG --- */

#ifndef GRID_DRAWING_STRATEGY
#define GRID_DRAWING_STRATEGY 1
#endif

#ifndef DRAW_EMPTY_ROWS
#define DRAW_EMPTY_ROWS 1
#endif

#ifndef SNAP_VIEW_TO_ROWS
#define SNAP_VIEW_TO_ROWS 1
#endif

#ifndef SMOOTH_SCROLL
#define SMOOTH_SCROLL 1
#endif

#ifndef FULL_WIDTH_HORIZ_LINES
#define FULL_WIDTH_HORIZ_LINES 1
#endif

/* Symlink behaviour */
#define SYMLINK_IGNORE 0
#define SYMLINK_LIST_SKIP_CONTENT 1
#define SYMLINK_LIST_RECURSE 2

#ifndef SYMLINK_BEHAVIOUR
#define SYMLINK_BEHAVIOUR SYMLINK_IGNORE
#endif

#ifndef SYMLINK_RECURSE_MAX_DEPTH
#define SYMLINK_RECURSE_MAX_DEPTH 32
#endif

/* Logging defaults */
#ifndef ERROR_LOG_PATH
#define ERROR_LOG_PATH "/var/log/bsuir-sp.log"
#endif

#ifndef ERROR_LOG_APPEND
#define ERROR_LOG_APPEND 0
#endif

#ifndef ERROR_LOG_FALLBACK
#define ERROR_LOG_FALLBACK "/dev/stderr"
#endif

#ifndef BATCH_SIZE
#define BATCH_SIZE 100
#endif

/* Permissions format */
#define PERM_SYMBOLIC 0
#define PERM_NUMERIC 1
#ifndef PERM_FORMAT
#define PERM_FORMAT PERM_SYMBOLIC
#endif

/* Show file type as first character in symbolic mode (e.g., 'd' for directory)
 */
#ifndef SHOW_FILE_TYPE
#define SHOW_FILE_TYPE 0
#endif

/* --- remaining original config --- */

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
  float row_height;
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

/* Logging file pointer (defined in globals.c) */
extern FILE *g_log_file;

/* Smooth scroll targets (defined in globals.c) */
extern float g_scroll_target_x;
extern float g_scroll_target_y;

/* Mutex for grid access */
extern SDL_Mutex *g_grid_mutex;

/* Max column widths */
extern int *g_max_col_widths;

/* FS traversal flag */
extern bool g_fs_traversing;

/* Stop flag for traversal */
extern volatile bool g_stop;

/* Prototypes */
void cleanup(void);
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color);
void set_cell(int row, int col, const char *text);
SizeAlloc sizeAllocate(int win_w, int win_h);
void draw_with_alloc(const SizeAlloc *sa);

/* Logging helpers */
int init_fs_log(void); /* returns 0 on success, 1 on fallback used */
void close_fs_log(void);
void log_fs_error(const char *fmt, ...);

/* FS thread functions (defined in fs.c) */
int traverse_fs(void *arg);

/* Event handling (defined in events.c) */
bool handle_events(SDL_Event *event, int win_w_local, int win_h_local);

/* Scroll handling (defined in scroll.c) */
void update_scroll(void);

/* main */
int main(int argc, char *argv[]);

#endif