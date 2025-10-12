#include "main.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

    int
    main(int argc, char *argv[]) {
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

  /* initialize smooth scroll targets to current offsets */
  g_scroll_target_x = g_offset_x;
  g_scroll_target_y = g_offset_y;

  bool running = true;
  SDL_Event event;

  /* animation / smoothing parameters */
  const float scroll_anim_factor = 0.22f; /* fraction per frame (0..1) */
  const float min_scroll_step =
      0.5f; /* minimal pixel change to continue animating */
  const int frame_delay_ms = 16; /* ~60 FPS */

  /* Main loop: poll events so we can animate smoothly between events */
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

    /* Process all pending events (non-blocking) */
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        running = false;
        break;

      case SDL_EVENT_KEY_DOWN:
        switch (event.key.key) {
        case SDLK_W:
          if (event.key.mod & SDL_KMOD_CTRL) {
            running = false;
          }
          break;
        case SDLK_Q:
          if (event.key.mod & SDL_KMOD_CTRL) {
            running = false;
          }
          break;
        case SDLK_UP:
          g_offset_y += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
          g_scroll_target_y = g_offset_y;
          break;
        case SDLK_DOWN:
          g_offset_y += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
          g_scroll_target_y = g_offset_y;
          break;
        case SDLK_LEFT:
          g_offset_x += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
          g_scroll_target_x = g_offset_x;
          break;
        case SDLK_RIGHT:
          g_offset_x += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
          g_scroll_target_x = g_offset_x;
          break;
        }
        break;

      case SDL_EVENT_MOUSE_WHEEL: {
#if SMOOTH_SCROLL
        /* wheel/touchpad: modify smooth target only if not dragging the
         * scrollbar */
        float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
        float dy = scroll_factor * event.wheel.y * SCROLL_SPEED;
        float dx = -scroll_factor * event.wheel.x * SCROLL_SPEED;
        if (!g_dragging_vert) {
          g_scroll_target_y += dy;
        } else {
          /* if user is dragging the scrollbar, update immediately and sync
           * target */
          g_offset_y += dy;
          g_scroll_target_y = g_offset_y;
        }
        if (!g_dragging_horz) {
          g_scroll_target_x += dx;
        } else {
          g_offset_x += dx;
          g_scroll_target_x = g_offset_x;
        }
#else
        {
          float scroll_factor = (NATURAL_SCROLL ? 1.0f : -1.0f);
          g_offset_y += scroll_factor * event.wheel.y * SCROLL_SPEED;
          g_offset_x += -scroll_factor * event.wheel.x * SCROLL_SPEED;
          g_scroll_target_x = g_offset_x;
          g_scroll_target_y = g_offset_y;
        }
#endif
      } break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
          int mx = event.button.x;
          int my = event.button.y;

#ifdef WITH_BORDER
          float border_local = BORDER_WIDTH;
#else
          float border_local = 0.0f;
#endif
          float view_w_local = win_w_local - 2 * border_local;
          float view_h_local = win_h_local - 2 * border_local;
          float content_w_local =
              view_w_local - (g_need_vert ? SCROLLBAR_WIDTH : 0);
          float content_h_local =
              view_h_local - (g_need_horz ? SCROLLBAR_WIDTH : 0);

          /* Vertical scrollbar thumb hit test */
          if (g_need_vert) {
            float vert_bar_x = border_local + content_w_local;
            float vert_bar_y = border_local;
            float vert_bar_h = content_h_local;
            float thumb_h = SDL_max(
                10.0f, content_h_local * (content_h_local / g_total_grid_h));

            float denom = SDL_max(0.0f, g_total_grid_h - content_h_local);
            float thumb_y;
            if (denom <= 0.0f) {
              thumb_y = vert_bar_y;
            } else {
              thumb_y =
                  vert_bar_y + (g_offset_y / denom) * (vert_bar_h - thumb_h);
            }

            if (mx >= vert_bar_x && mx < vert_bar_x + SCROLLBAR_WIDTH &&
                my >= thumb_y && my < thumb_y + thumb_h) {
              g_dragging_vert = true;
              g_drag_start_pos = my;
              g_drag_start_offset = g_offset_y;
              /* ensure target synced while dragging (no smooth animation) */
              g_scroll_target_y = g_offset_y;
            }
          }

          /* Horizontal scrollbar thumb hit test */
          if (g_need_horz) {
            float horz_bar_x = border_local;
            float horz_bar_y = border_local + content_h_local;
            float horz_bar_w = content_w_local;
            float thumb_w = SDL_max(
                10.0f, content_w_local * (content_w_local / g_total_grid_w));

            float denom = SDL_max(0.0f, g_total_grid_w - content_w_local);
            float thumb_x;
            if (denom <= 0.0f) {
              thumb_x = horz_bar_x;
            } else {
              thumb_x =
                  horz_bar_x + (g_offset_x / denom) * (horz_bar_w - thumb_w);
            }

            if (mx >= thumb_x && mx < thumb_x + thumb_w && my >= horz_bar_y &&
                my < horz_bar_y + SCROLLBAR_WIDTH) {
              g_dragging_horz = true;
              g_drag_start_pos = mx;
              g_drag_start_offset = g_offset_x;
              g_scroll_target_x = g_offset_x;
            }
          }
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        if (g_dragging_vert) {
          int win_h_local2;
          SDL_GetWindowSize(g_window, NULL, &win_h_local2);
#ifdef WITH_BORDER
          float border_local = BORDER_WIDTH;
#else
          float border_local = 0.0f;
#endif
          float view_h_local = win_h_local2 - 2 * border_local;
          float content_h_local =
              view_h_local - (g_need_horz ? SCROLLBAR_WIDTH : 0);
          float vert_bar_h = content_h_local;
          float thumb_h = SDL_max(
              10.0f, content_h_local * (content_h_local / g_total_grid_h));
          float max_offset_y = SDL_max(0.0f, g_total_grid_h - content_h_local);
          float track_h = vert_bar_h - thumb_h;
          float dy = event.motion.y - g_drag_start_pos;
          float scroll_factor =
              (track_h > 0.0f) ? (max_offset_y / track_h) : 0.0f;
          g_offset_y = g_drag_start_offset + dy * scroll_factor;
          g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);
          /* sync target so animation doesn't fight dragging */
          g_scroll_target_y = g_offset_y;
        } else if (g_dragging_horz) {
          int win_w_local2;
          SDL_GetWindowSize(g_window, &win_w_local2, NULL);
#ifdef WITH_BORDER
          float border_local = BORDER_WIDTH;
#else
          float border_local = 0.0f;
#endif
          float view_w_local = win_w_local2 - 2 * border_local;
          float content_w_local =
              view_w_local - (g_need_vert ? SCROLLBAR_WIDTH : 0);
          float horz_bar_w = content_w_local;
          float thumb_w = SDL_max(
              10.0f, content_w_local * (content_w_local / g_total_grid_w));
          float max_offset_x = SDL_max(0.0f, g_total_grid_w - content_w_local);
          float track_w = horz_bar_w - thumb_w;
          float dx = event.motion.x - g_drag_start_pos;
          float scroll_factor =
              (track_w > 0.0f) ? (max_offset_x / track_w) : 0.0f;
          g_offset_x = g_drag_start_offset + dx * scroll_factor;
          g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
          g_scroll_target_x = g_offset_x;
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          g_dragging_vert = false;
          g_dragging_horz = false;
        }
        break;
      } /* switch event.type */
    } /* while PollEvent */

    /* Smooth scrolling: move current offset towards target if allowed and not
     * dragging */
#if SMOOTH_SCROLL
    if (!g_dragging_vert) {
      float dy = g_scroll_target_y - g_offset_y;
      if (fabsf(dy) > min_scroll_step) {
        g_offset_y += dy * scroll_anim_factor;
      } else {
        g_offset_y = g_scroll_target_y;
      }
    }
    if (!g_dragging_horz) {
      float dx = g_scroll_target_x - g_offset_x;
      if (fabsf(dx) > min_scroll_step) {
        g_offset_x += dx * scroll_anim_factor;
      } else {
        g_offset_x = g_scroll_target_x;
      }
    }
#endif

    /* If SNAP_VIEW_TO_ROWS enabled — snap offsets at boundaries so start/end
     * never show half-row */
#if SNAP_VIEW_TO_ROWS
    {
      float max_off_y = SDL_max(0.0f, sa.total_grid_h - sa.content_h);
      /* small epsilon to decide snapping; choose 1 pixel (safe) */
      const float snap_eps = 1.0f;
      if (g_offset_y >= 0.0f && g_offset_y <= snap_eps) {
        g_offset_y = 0.0f;
        g_scroll_target_y = 0.0f;
      }
      if (fabsf(g_offset_y - max_off_y) <= snap_eps) {
        g_offset_y = max_off_y;
        g_scroll_target_y = max_off_y;
      }
    }
#endif

    /* After handling input & potential animation, draw frame using the
     * precomputed layout. */
    draw_with_alloc(&sa);

    /* Free the arrays allocated by sizeAllocate */
    free(sa.col_widths);
    free(sa.col_left);
    SDL_UnlockMutex(g_grid_mutex);

    /* Sleep a bit to cap frame rate and allow animation */
    SDL_Delay(frame_delay_ms);
  }

  g_stop = true;

  SDL_WaitThread(fs_thread, NULL);

  return 0;
}