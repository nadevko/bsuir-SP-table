/* include/globals.h */
#pragma once

#include "types.h"
#include "virtual_scroll.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>

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

/* Selection state (indexing) */
extern int g_selected_row;
extern int g_selected_col;
extern int g_selected_index;

/* Row height and column geometry cached for event hit-testing */
extern float g_row_height;
extern float *g_col_left;
extern int *g_col_widths;

/* Virtual scrolling */
extern VirtualScrollState *g_vscroll;
extern float g_last_content_h;

/* Prototypes */
void cleanup(void);
