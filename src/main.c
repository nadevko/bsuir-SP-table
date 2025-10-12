#include "main.h"
#include <errno.h>
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

  /* Initialize filesystem error log early */
  init_fs_log();

  /* Set grid dimensions (start with 1 row for headers) */
  g_cols = DEFAULT_COLS; // 4 columns: File, Size (bytes), Date, Permissions
  g_rows = 1;

  atexit(cleanup);

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed");

  SDL_CHECK(TTF_Init(), "SDL-ttf initialisation failed");
#ifdef WITH_FONTCONFIG
  FcConfig *fontconfig;
  ANY_CHECK(fontconfig = FcInitLoadConfigAndFonts(),
            "FcConfig initialisation failed");

  FcPattern *pattern;
  ANY_CHECK(pattern = FcNameParse((const FcChar8 *)CELL_TEXT_NAME),
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

  g_font = TTF_OpenFont((const char *)file, CELL_TEXT_SIZE);

  FcPatternDestroy(pattern);
  FcPatternDestroy(match);
  FcConfigDestroy(fontconfig);
#else
  g_font = TTF_OpenFont(CELL_TEXT_NAME, CELL_TEXT_SIZE);
#endif
  if (!g_font) {
    log_fs_error("Failed to load font: %s", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Allocate grid */
  g_grid = malloc(g_rows * sizeof(Cell *));
  if (!g_grid) {
    log_fs_error("Failed to allocate memory for grid");
    if (argc == 1)
      free(dir_path);
    return 1;
  }
  for (int r = 0; r < g_rows; r++) {
    g_grid[r] = calloc(g_cols, sizeof(Cell));
    if (!g_grid[r]) {
      log_fs_error("Failed to allocate memory for grid row %d", r);
      for (int i = 0; i < r; i++)
        free(g_grid[i]);
      free(g_grid);
      g_grid = NULL;
      if (argc == 1)
        free(dir_path);
      return 1;
    }
  }

  /* Set header row */
  set_cell(0, 0, "File");
  set_cell(0, 1, "Size (bytes)");
  set_cell(0, 2, "Date");
  set_cell(0, 3, "Permissions");

  /* Initialize max column widths from header */
  g_max_col_widths = calloc(g_cols, sizeof(int));
  if (!g_max_col_widths) {
    log_fs_error("Failed to allocate memory for max_col_widths");
    if (argc == 1)
      free(dir_path);
    return 1;
  }
  for (int c = 0; c < g_cols; c++) {
    g_max_col_widths[c] = g_grid[0][c].text_width;
  }

  /* Create mutex for grid access */
  g_grid_mutex = SDL_CreateMutex();
  if (!g_grid_mutex) {
    log_fs_error("Failed to create mutex: %s", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Start filesystem traversal thread */
  char *thread_dir = strdup(dir_path);
  g_fs_traversing = true;
  g_stop = false;
  SDL_Thread *fs_thread =
      SDL_CreateThread(traverse_fs, "FS Traversal", thread_dir);
  if (!fs_thread) {
    log_fs_error("Failed to create thread: %s", SDL_GetError());
    free(thread_dir);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  if (argc == 1)
    free(dir_path);

  /* Create window & renderer */
  SDL_CHECK(SDL_CreateWindowAndRenderer("Directory Listing", 800, 600,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  /* Initialize smooth scroll targets to current offsets */
  g_scroll_target_x = g_offset_x;
  g_scroll_target_y = g_offset_y;

  bool running = true;
  SDL_Event event;
  const int frame_delay_ms = 16; /* ~60 FPS */

  /* Main loop: poll events and update display */
  while (running) {
    /* Get current window size and compute layout */
    int win_w_local = 0, win_h_local = 0;
    SDL_GetWindowSize(g_window, &win_w_local, &win_h_local);

    SDL_LockMutex(g_grid_mutex);
    SizeAlloc sa = sizeAllocate(win_w_local, win_h_local);

    /* Update global copies used by event handlers */
    g_need_horz = sa.need_horz;
    g_need_vert = sa.need_vert;
    g_total_grid_w = sa.total_grid_w;
    g_total_grid_h = sa.total_grid_h;
    g_content_w = sa.content_w;
    g_content_h = sa.content_h;

    /* Process all pending events */
    while (SDL_PollEvent(&event)) {
      if (handle_events(&event, win_w_local, win_h_local)) {
        running = false;
      }
    }

    /* Update scrolling */
    update_scroll();

    /* Draw frame using the precomputed layout */
    draw_with_alloc(&sa);

    /* Free the arrays allocated by sizeAllocate */
    free(sa.col_widths);
    free(sa.col_left);
    SDL_UnlockMutex(g_grid_mutex);

    /* Cap frame rate */
    SDL_Delay(frame_delay_ms);
  }

  g_stop = true;
  SDL_WaitThread(fs_thread, NULL);

  return 0;
}