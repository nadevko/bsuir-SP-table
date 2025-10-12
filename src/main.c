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

  /* Count files in directory to set row count */
  int file_count = count_files(dir_path);

  /* Set grid dimensions (add 1 row for headers) */
  g_rows = file_count + 1; // +1 for header row
  g_cols = DEFAULT_COLS;   // 4 columns: File, Size (bytes), Date, Permissions

  if (file_count == 0) {
    fprintf(stderr, "No files found in directory %s\n", dir_path);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

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
    fprintf(stderr, "Failed to load font: %s\n", SDL_GetError());
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Allocate grid */
  g_grid = malloc(g_rows * sizeof(Cell *));
  if (!g_grid) {
    fprintf(stderr, "Failed to allocate memory for grid\n");
    if (argc == 1)
      free(dir_path);
    return 1;
  }
  for (int r = 0; r < g_rows; r++) {
    g_grid[r] = calloc(g_cols, sizeof(Cell));
    if (!g_grid[r]) {
      fprintf(stderr, "Failed to allocate memory for grid row %d\n", r);
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

  /* Populate grid with file information */
  populate_files(dir_path, 1);

  if (argc == 1)
    free(dir_path);

  /* Create window & renderer */
  SDL_CHECK(SDL_CreateWindowAndRenderer("Directory Listing", 800, 600,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  bool running = true;
  SDL_Event event;

  /* Main loop:
   *  - compute layout once per frame via sizeAllocate()
   *  - update globals used by event code
   *  - handle events (mouse, keyboard)
   *  - draw using the precomputed layout
   */
  while (running) {
    /* Get current window size and compute layout */
    int win_w_local = 0, win_h_local = 0;
    SDL_GetWindowSize(g_window, &win_w_local, &win_h_local);

    SizeAlloc sa = sizeAllocate(win_w_local, win_h_local);

    /* Update global copies used by event handlers */
    g_need_horz = sa.need_horz;
    g_need_vert = sa.need_vert;
    g_total_grid_w = sa.total_grid_w;
    g_total_grid_h = sa.total_grid_h;
    g_content_w = sa.content_w;
    g_content_h = sa.content_h;

    /* Wait for next event and process it. We keep WaitEvent to reduce CPU
     * usage. Because we recompute layout before waiting, event handlers can use
     * correct sizes.
     */
    SDL_WaitEvent(&event);

    switch (event.type) {
    case SDL_EVENT_QUIT:
      running = false;
      break;

    case SDL_EVENT_KEY_DOWN:
      switch (event.key.key) {
      case SDLK_ESCAPE:
        running = false;
        break;
      case SDLK_UP:
        g_offset_y += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
        break;
      case SDLK_DOWN:
        g_offset_y += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
        break;
      case SDLK_LEFT:
        g_offset_x += (NATURAL_SCROLL ? SCROLL_SPEED : -SCROLL_SPEED);
        break;
      case SDLK_RIGHT:
        g_offset_x += (NATURAL_SCROLL ? -SCROLL_SPEED : SCROLL_SPEED);
        break;
      }
      break;

    case SDL_EVENT_MOUSE_WHEEL: {
      float scroll_factor = NATURAL_SCROLL ? 1.0f : -1.0f;
      g_offset_y += scroll_factor * event.wheel.y * SCROLL_SPEED;
      g_offset_x += -scroll_factor * event.wheel.x * SCROLL_SPEED;
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
        float thumb_h = SDL_max(10.0f, content_h_local *
                                           (content_h_local / g_total_grid_h));
        float max_offset_y = SDL_max(0.0f, g_total_grid_h - content_h_local);
        float track_h = vert_bar_h - thumb_h;
        float dy = event.motion.y - g_drag_start_pos;
        float scroll_factor =
            (track_h > 0.0f) ? (max_offset_y / track_h) : 0.0f;
        g_offset_y = g_drag_start_offset + dy * scroll_factor;
        g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);
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
        float thumb_w = SDL_max(10.0f, content_w_local *
                                           (content_w_local / g_total_grid_w));
        float max_offset_x = SDL_max(0.0f, g_total_grid_w - content_w_local);
        float track_w = horz_bar_w - thumb_w;
        float dx = event.motion.x - g_drag_start_pos;
        float scroll_factor =
            (track_w > 0.0f) ? (max_offset_x / track_w) : 0.0f;
        g_offset_x = g_drag_start_offset + dx * scroll_factor;
        g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        g_dragging_vert = false;
        g_dragging_horz = false;
      }
      break;
    }

    /* After handling input, draw frame using the precomputed layout. */
    draw_with_alloc(&sa);

    /* Free the arrays allocated by sizeAllocate */
    free(sa.col_widths);
    free(sa.col_left);
  }

  return 0;
}
