#pragma once
#include <SDL3/SDL.h>

#define SYMLINK_IGNORE 0
#define SYMLINK_LIST_SKIP_CONTENT 1
#define SYMLINK_LIST_RECURSE 2

#define SYMLINK_BEHAVIOUR SYMLINK_IGNORE
#define SYMLINK_RECURSE_MAX_DEPTH 32

#define ERROR_LOG_PATH "./bsuir-sp.log"
#define ERROR_LOG_MODE "w"
#define ERROR_LOG_FALLBACK "/dev/stderr"

#define BATCH_SIZE 100

/* Template for PERM_SYMBOLIC format:
 * %n - numeric permissions ([0-6]{4})
 * %T - file type (d/l/-/c/b/p/s/?)
 * %S - special bits separate (--- to sst)
 * %u - user permissions (rwx)
 * %g - group permissions (rwx)
 * %o - other permissions (rwx)
 * %U - user with embedded setuid (rws/rwS)
 * %G - group with embedded setgid (rws/rwS)
 * %O - other with embedded sticky (rwt/rwT)
 *
 * Examples:
 * "%T%U%G%O"        -> "drwsr-xr-t"  (classic ls -l)
 * "%S %T%u %g %o"   -> "s-- drwx r-x r--"
 * "%T%u%g%o"        -> "drwxr-xr--"  (no special bits)
 * "%u%g%o"          -> "rwxr-xr--"   (no type)
 */
#define PERM_TEMPLATE "%S %T %u %g %o"

// #define SHOW_FILE_RELATIVE_PATH

#define WITH_BORDER
#define BORDER_COLOUR (SDL_Color){100, 100, 100, 255}
#define BORDER_WIDTH 2

#define WITH_GRID
#define GRID_BACKGROUND_COLOUR (SDL_Color){240, 240, 240, 255}
#define GRID_LINE_COLOUR BORDER_COLOUR
#define GRID_LINE_WIDTH (float)BORDER_WIDTH

/* Highlight (selection) configuration: цвет рамки выделенной ячейки и её
 * толщина */
#define HIGHLIGHT_BORDER_COLOUR (SDL_Color){200, 0, 0, 255}
#define HIGHLIGHT_BORDER_WIDTH 3

#define WITH_FONTCONFIG

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

#define SMOOTH_SCROLL
#define SCROLL_FACTOR 1.0f

/* DATE_FORMAT_TEMPLATE is a strftime()-style format string used to format
 * file modification times shown in the grid.
 *
 * By default "%c" is used which prints the date/time in the program's locale.
 * You can override this macro to use other formats.
 */
#define DATE_FORMAT_TEMPLATE "%H:%M:%S %d %B %Y"

/* --- Header templates (user-configurable) ---
 * Supported substitutions:
 *  %% -> literal '%'
 *  %P -> canonical (resolved) path to the directory passed to the program
 *  %p -> original (as-passed) path to the directory
 *  %b -> sum of all displayed sizes (st.st_size for each entry: files +
 * directories) Note: not "true disk usage", just sum of what's shown in the
 * table %f -> sum of regular files only (excludes directories) %d -> actual
 * disk usage (st.st_blocks accounting for filesystem block size) Includes inode
 * sizes, block allocation, fragmentation
 *
 * By default we provide sensible labels; you can change these constants
 * (or override them at build time).
 */
#define HEADER_TEMPLATE_0 "File at %P"
#define HEADER_TEMPLATE_1 "Size (bytes) %b"
#define HEADER_TEMPLATE_2 "Date"
#define HEADER_TEMPLATE_3 "Permissions"
