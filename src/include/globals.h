#pragma once
/* globals.h — global variables and cleanup */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>

#include "types.h"

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

/* Logging file pointer */
extern FILE *g_log_file;

/* Smooth scroll targets */
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
