#pragma once
#include <SDL3/SDL.h>

#define GRID_DRAWING_STRATEGY 0
#define DRAW_EMPTY_ROWS 1
#define SNAP_VIEW_TO_ROWS 1
#define SMOOTH_SCROLL 1
#define FULL_WIDTH_HORIZ_LINES 1

/* Symlink behaviour */
#define SYMLINK_IGNORE 0
#define SYMLINK_LIST_SKIP_CONTENT 1
#define SYMLINK_LIST_RECURSE 2
#define SYMLINK_BEHAVIOUR SYMLINK_IGNORE
#define SYMLINK_RECURSE_MAX_DEPTH 32

#define ERROR_LOG_PATH "./bsuir-sp.log"
#define ERROR_LOG_APPEND 0
#define ERROR_LOG_FALLBACK "/dev/stderr"

#define BATCH_SIZE 100

/* Permissions format */
#define PERM_SYMBOLIC 0
#define PERM_NUMERIC 1
#define PERM_FORMAT PERM_SYMBOLIC

#define SHOW_FILE_TYPE 0

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

/* Highlight (selection) configuration: цвет рамки выделенной ячейки и её
 * толщина */
#define HIGHLIGHT_BORDER_COLOUR (SDL_Color){200, 0, 0, 255}
#define HIGHLIGHT_BORDER_WIDTH 3

#define WITH_FONTCONFIG
#define RECURSIVE_LISTING

#define LEFT 0
#define CENTER 1
#define RIGHT 2
#define TOP 0
#define BOTTOM 2

#define TEXT_FONT_NAME "Ubuntu Mono"
#define TEXT_FONT_SIZE 32.0
#define TEXT_FONT_POSITION_HORIZONTAL LEFT
#define TEXT_FONT_POSITION_VERTICAL BOTTOM
#define TEXT_FONT_COLOUR (SDL_Color){100, 100, 100, 255}
#define CELL_PADDING 10

#define SCROLLBAR_WIDTH 20
#define SCROLL_SPEED 50
#define SCROLLBAR_BG_COLOUR (SDL_Color){200, 200, 200, 255}
#define SCROLLBAR_THUMB_COLOUR (SDL_Color){100, 100, 100, 255}

#define NATURAL_SCROLL 0
