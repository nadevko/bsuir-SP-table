#include "main.h"
#include <stdlib.h>

/* Definitions of globals (previously 'static' in the big file) */
SDL_Renderer *g_renderer = NULL;
SDL_Window *g_window = NULL;
TTF_Font *g_font = NULL;
Cell **g_grid = NULL;

int g_rows = DEFAULT_ROWS;
int g_cols = DEFAULT_COLS;

float g_offset_x = 0.0f;
float g_offset_y = 0.0f;

bool g_dragging_vert = false;
bool g_dragging_horz = false;
float g_drag_start_pos = 0.0f;
float g_drag_start_offset = 0.0f;

bool g_need_horz = false;
bool g_need_vert = false;
float g_total_grid_w = 0.0f;
float g_total_grid_h = 0.0f;
float g_content_w = 0.0f;
float g_content_h = 0.0f;
float g_view_x = 0.0f;
float g_view_y = 0.0f;
float g_view_w = 0.0f;
float g_view_h = 0.0f;
