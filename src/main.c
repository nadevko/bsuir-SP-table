#include "include/main.h"
#include "include/columns.h"
#include "include/config.h"
#include "include/events.h"
#include "include/fs.h"
#include "include/globals.h"
#include "include/grid.h"
#include "include/layout.h"
#include "include/provider.h"
#include "include/scroll.h"
#include "include/table_model.h"
#include "include/utils.h"
#include "include/virtual_scroll.h"
#include <errno.h>
#include <fontconfig/fontconfig.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *dir_path = NULL;
  if (argc == 2) {
    dir_path = argv[1];
  } else if (argc == 1) {
    dir_path = getcwd(NULL, 0);
    if (!dir_path) {
      fprintf(stderr, "Failed to get current working directory: %s\n",
              strerror(errno));
      return 1;
    }
  } else {
    fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
    return 1;
  }

  char *loc = setlocale(LC_ALL, "");
  if (!loc) {
    fprintf(stderr,
            "Warning: setlocale(LC_ALL, \"\") failed — using \"C\" locale\n");
  } else {
    fprintf(stderr, "Locale set to: %s\n", loc);
  }

  init_fs_log();

  atexit(cleanup);

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed");
  SDL_CHECK(TTF_Init(), "SDL-ttf initialisation failed");
#ifdef WITH_FONTCONFIG
  FcConfig *fontconfig;
  ANY_CHECK(fontconfig = FcInitLoadConfigAndFonts(),
            "FcConfig initialisation failed");
  FcPattern *pattern;
  ANY_CHECK(pattern = FcNameParse((const FcChar8 *)TEXT_FONT_NAME),
            "Failed to parse font name");
  FcConfigSubstitute(fontconfig, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match;
  ANY_CHECK(match = FcFontMatch(fontconfig, pattern, &result),
            "Failed to resolve font");

  FcChar8 *file = NULL;
  ANY_CHECK(FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch,
            "Failed to find font file");

  g_font = TTF_OpenFont((const char *)file, TEXT_FONT_SIZE);
  FcPatternDestroy(pattern);
  FcPatternDestroy(match);
  FcConfigDestroy(fontconfig);
#else
  g_font = TTF_OpenFont(TEXT_FONT_NAME, TEXT_FONT_SIZE);
#endif
  if (!g_font) {
    fprintf(stderr, "Failed to load font: %s\n", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  g_grid_mutex = SDL_CreateMutex();
  if (!g_grid_mutex) {
    fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* --- Create table model --- */
  DataProvider *provider = provider_create_filesystem(dir_path);
  if (!provider) {
    fprintf(stderr, "Failed to create filesystem provider\n");
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  ColumnRegistry *cols = cols_create();
  if (!cols) {
    fprintf(stderr, "Failed to create column registry\n");
    provider_destroy(provider);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  cols_add(cols, col_path_default());
  cols_add(cols, col_size_default());
  cols_add(cols, col_date_default());
  cols_add(cols, col_perms_default());

  g_table = table_create(provider, cols);
  if (!g_table) {
    fprintf(stderr, "Failed to create table model\n");
    cols_destroy(cols);
    provider_destroy(provider);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  g_cols = table_get_col_count(g_table);
  fprintf(stderr, "Table created with %d columns\n", g_cols);

  /* --- Legacy grid support (minimal setup for compatibility) --- */
  g_rows = 1; /* Header row */
  g_grid = malloc((size_t)g_rows * sizeof *g_grid);
  if (!g_grid) {
    fprintf(stderr, "Failed to allocate memory for grid\n");
    table_destroy(g_table);
    g_table = NULL;
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  g_grid[0] = calloc((size_t)g_cols, sizeof *g_grid[0]);
  if (!g_grid[0]) {
    fprintf(stderr, "Failed to allocate memory for grid row\n");
    free(g_grid);
    g_grid = NULL;
    table_destroy(g_table);
    g_table = NULL;
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  g_max_col_widths = calloc((size_t)g_cols, sizeof *g_max_col_widths);
  if (!g_max_col_widths) {
    fprintf(stderr, "Failed to allocate memory for max_col_widths\n");
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Initialize headers from table */
  for (int c = 0; c < g_cols; c++) {
    char *header = table_get_cell(g_table, -1, c);
    if (header) {
      set_cell(0, c, header);
      free(header);
    }
  }

  g_vscroll = vscroll_init(g_cols);
  if (!g_vscroll) {
    fprintf(stderr, "Failed to initialize virtual scrolling\n");
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  fprintf(stderr, "Virtual scroll initialized\n");

  char *thread_dir = strdup(dir_path);

  g_fs_traversing = true;
  g_stop = false;
  SDL_Thread *fs_thread =
      SDL_CreateThread(traverse_fs, "FS Traversal", thread_dir);
  if (!fs_thread) {
    fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
    free(thread_dir);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  if (argc == 1)
    free(dir_path);

  fprintf(stderr, "Creating window and renderer...\n");
  SDL_CHECK(SDL_CreateWindowAndRenderer("Directory Listing", 800, 600,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  fprintf(stderr, "Window created\n");

  g_scroll_target_x = g_offset_x;
  g_scroll_target_y = g_offset_y;

  bool running = true;

  SDL_Event event;
  const int frame_delay_ms = 16;
  unsigned long long last_total_bytes = 0ULL;

  fprintf(stderr, "Entering main loop\n");

  while (running) {
    int win_w_local = 0, win_h_local = 0;
    SDL_GetWindowSize(g_window, &win_w_local, &win_h_local);

    fprintf(stderr, "DEBUG: Checking vscroll initialization\n");
    if (!g_vscroll) {
      fprintf(stderr, "ERROR: g_vscroll is NULL!\n");
      SDL_UnlockMutex(g_grid_mutex);
      break;
    }

    if (!g_table) {
      fprintf(stderr, "ERROR: g_table is NULL in main loop\n");
      SDL_UnlockMutex(g_grid_mutex);
      break;
    }

    SDL_LockMutex(g_grid_mutex);

    /* Update headers if any totals changed */
    if (g_total_bytes != last_total_bytes) {
      if (g_grid && g_grid[0]) {
        for (int c = 0; c < g_cols; ++c) {
          char *header = table_get_cell(g_table, -1, c);
          if (header) {
            set_cell_with_width_update(0, c, header);
            free(header);
          }
        }
      }
      last_total_bytes = g_total_bytes;
    }

    /* Recalculate column widths if dirty */
    if (table_is_widths_dirty(g_table)) {
      table_recalc_widths(g_table, g_font, CELL_PADDING);
    }

    /* IMPORTANT: sizeAllocate AFTER updating headers */
    SizeAlloc sa = sizeAllocate(win_w_local, win_h_local);

    /* Clamp scroll offsets to valid range after layout recalculation */
    scroll_clamp_all();

    /* Update virtual scroll with actual table row count */
    int table_rows = table_get_row_count(g_table);
    int total_display_rows = table_rows + 1; /* +1 for header */

    if (g_vscroll->total_virtual_rows != total_display_rows) {
      g_vscroll->total_virtual_rows = total_display_rows;
    }

    /* g_rows is only buffer size, not total rows */
    g_rows = VSCROLL_BUFFER_SIZE + 1;

    fprintf(stderr, "DEBUG: vscroll_update_buffer_position\n");
    if (g_vscroll && g_content_h > 0 && sa.row_height > 0) {
      vscroll_update_buffer_position(g_vscroll, g_view_y, g_content_h,
                                     sa.row_height);
    }

    fprintf(stderr, "DEBUG: checking g_vscroll->needs_reload\n");
    if (g_vscroll && g_vscroll->needs_reload) {
      fprintf(stderr, "DEBUG: vscroll needs reload, marking as complete\n");
      g_vscroll->needs_reload = false;
      g_vscroll->buffer_start_row = g_vscroll->desired_start_row;
      g_vscroll->buffer_count = VSCROLL_BUFFER_SIZE;
    }

    g_need_horz = sa.need_horz;
    g_need_vert = sa.need_vert;
    g_total_grid_w = sa.total_grid_w;
    g_total_grid_h = sa.total_grid_h;
    g_content_w = sa.content_w;
    g_content_h = sa.content_h;

    g_row_height = sa.row_height;

    if (g_col_left) {
      free(g_col_left);
      g_col_left = NULL;
    }
    if (g_col_widths) {
      free(g_col_widths);
      g_col_widths = NULL;
    }

    if (g_cols > 0) {
      g_col_left = malloc((size_t)g_cols * sizeof *g_col_left);
      g_col_widths = malloc((size_t)g_cols * sizeof *g_col_widths);
      if (g_col_left && g_col_widths) {
        for (int c = 0; c < g_cols; c++) {
          g_col_left[c] = sa.col_left[c];
          g_col_widths[c] = sa.col_widths[c];
        }
      } else {
        if (g_col_left) {
          free(g_col_left);
          g_col_left = NULL;
        }
        if (g_col_widths) {
          free(g_col_widths);
          g_col_widths = NULL;
        }
      }
    }

    while (SDL_PollEvent(&event)) {
      if (handle_events(&event, win_w_local, win_h_local)) {
        running = false;
      }
    }

    update_scroll();

    draw_with_alloc(&sa);

    free(sa.col_widths);
    free(sa.col_left);
    SDL_UnlockMutex(g_grid_mutex);

    SDL_Delay(frame_delay_ms);
  }

  fprintf(stderr, "Exiting main loop\n");
  g_stop = true;
  SDL_WaitThread(fs_thread, NULL);
  return 0;
}