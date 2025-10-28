#include "include/main.h"
#include "include/config.h"
#include "include/events.h"
#include "include/fs.h"
#include "include/globals.h"
#include "include/grid.h"
#include "include/layout.h"
#include "include/scroll.h"
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

  /* --- Включаем локаль из окружения. Это обязательно, чтобы strftime()
   *     использовал локализованные названия месяцев/дней и формат %c.
   *     Должно быть выполнено ДО создания потоков и ДО вызовов, зависящих
   *     от локали. */
  char *loc = setlocale(LC_ALL, "");
  if (!loc) {
    fprintf(stderr,
            "Warning: setlocale(LC_ALL, \"\") failed — using \"C\" locale\n");
  } else {
    fprintf(stderr, "Locale set to: %s\n", loc);
  }

  init_fs_log();

  g_cols = 4;
  g_rows = 1; /* Начинаем с 1 строки - заголовок */
  fprintf(stderr, "INIT: g_rows = %d (header row)\n", g_rows);
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
    log_fs_error("Failed to load font: %s", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

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

  /* Initialize max column widths BEFORE setting header cells */
  g_max_col_widths = calloc(g_cols, sizeof(int));
  if (!g_max_col_widths) {
    log_fs_error("Failed to allocate memory for max_col_widths");
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Заголовок: показываем канонический путь (реальный путь). */
  char *resolved = realpath(dir_path, NULL);
  if (!resolved) {
    resolved = strdup(dir_path ? dir_path : ".");
    if (!resolved) {
      log_fs_error("Failed to allocate memory for resolved path");
      set_cell(0, 0, "File");
    } else {
      size_t len = strlen(resolved) + 12; /* "File at ()\0" запас */
      char *header = malloc(len);
      if (!header) {
        log_fs_error("Failed to allocate memory for header");
        set_cell(0, 0, "File");
        free(resolved);
      } else {
        snprintf(header, len, "File at %s", resolved);
        set_cell(0, 0, header);
        free(header);
        free(resolved);
      }
    }
  } else {
    size_t len = strlen(resolved) + 12;
    char *header = malloc(len);
    if (!header) {
      log_fs_error("Failed to allocate memory for header");
      set_cell(0, 0, "File");
      free(resolved);
    } else {
      snprintf(header, len, "File at %s", resolved);
      set_cell(0, 0, header);
      free(header);
      free(resolved);
    }
  }

  /* These set_cell calls now automatically update g_max_col_widths! */
  set_cell(0, 1, "Size (bytes)");
  set_cell(0, 2, "Date");
  set_cell(0, 3, "Permissions");

  g_grid_mutex = SDL_CreateMutex();
  if (!g_grid_mutex) {
    log_fs_error("Failed to create mutex: %s", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  g_vscroll = vscroll_init(g_cols);
  if (!g_vscroll) {
    log_fs_error("Failed to initialize virtual scrolling");
    if (argc == 1)
      free(dir_path);
    return 1;
  }

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

  SDL_CHECK(SDL_CreateWindowAndRenderer("Directory Listing", 800, 600,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  g_scroll_target_x = g_offset_x;
  g_scroll_target_y = g_offset_y;

  bool running = true;

  SDL_Event event;
  const int frame_delay_ms = 16;
  unsigned long long last_total_bytes = 0ULL;

  while (running) {
    int win_w_local = 0, win_h_local = 0;
    SDL_GetWindowSize(g_window, &win_w_local, &win_h_local);
    SDL_LockMutex(g_grid_mutex);

    /* Update headers if any totals changed */
    if (g_total_bytes != last_total_bytes) {
      if (g_grid && g_grid[0]) {
        /* Re-render all header templates to reflect new totals */
        const char *templates[] = {HEADER_TEMPLATE_0, HEADER_TEMPLATE_1,
                                   HEADER_TEMPLATE_2, HEADER_TEMPLATE_3};
        for (int c = 0; c < g_cols; ++c) {
          const char *tpl =
              (c < (int)(sizeof(templates) / sizeof(templates[0])))
                  ? templates[c]
                  : NULL;
          char *rendered = render_header_template(tpl ? tpl : "");
          if (rendered) {
            set_cell_with_width_update(0, c, rendered);
            free(rendered);
          }
        }
      }
      last_total_bytes = g_total_bytes;
    }

    /* IMPORTANT: sizeAllocate AFTER updating headers, so g_max_col_widths is
     * current */
    SizeAlloc sa = sizeAllocate(win_w_local, win_h_local);

    /* Clamp scroll offsets to valid range after layout recalculation */
    scroll_clamp_all();

    /* КРИТИЧЕСКАЯ СИНХРОНИЗАЦИЯ: virtual scroll должен знать точное кол-во
     * строк */
    if (g_vscroll->total_virtual_rows != g_rows) {
      fprintf(stderr, "SYNC ERROR: vscroll thinks %d rows, but g_rows=%d\n",
              g_vscroll->total_virtual_rows, g_rows);
      g_vscroll->total_virtual_rows = g_rows;
    }
    vscroll_update_buffer_position(g_vscroll, g_view_y, g_content_h,
                                   sa.row_height);

    if (g_vscroll->needs_reload) {
      vscroll_load_from_grid(g_vscroll, g_vscroll->desired_start_row,
                             VSCROLL_BUFFER_SIZE);
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
      g_col_left = malloc(sizeof(float) * g_cols);
      g_col_widths = malloc(sizeof(int) * g_cols);
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

  g_stop = true;
  SDL_WaitThread(fs_thread, NULL);
  return 0;
}