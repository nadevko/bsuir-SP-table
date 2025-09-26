#include "main.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Clean-up helper */
void cleanup(void) {
  if (g_font != NULL) {
    TTF_CloseFont(g_font);
    g_font = NULL;
  }

  if (g_grid != NULL) {
    for (int r = 0; r < g_rows; r++) {
      for (int c = 0; c < g_cols; c++) {
        free(g_grid[r][c].text);
      }
      free(g_grid[r]);
    }
    free(g_grid);
    g_grid = NULL;
  }

  if (g_renderer != NULL) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = NULL;
  }
  if (g_window != NULL) {
    SDL_DestroyWindow(g_window);
    g_window = NULL;
  }
  SDL_Quit();
}

/* Utility wrapper (keeps your naming) */
bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) ==
         0;
}

/* Set a cell (update string + measured size). */
void set_cell(int row, int col, const char *text) {
  if (row < 0 || row >= g_rows || col < 0 || col >= g_cols)
    return;
  free(g_grid[row][col].text);
  g_grid[row][col].text = strdup(text ? text : "");
  size_t len = strlen(g_grid[row][col].text);
  bool success = TTF_GetStringSize(g_font, g_grid[row][col].text, len,
                                   &g_grid[row][col].text_width,
                                   &g_grid[row][col].text_height);
  if (!success) {
    g_grid[row][col].text_width = 0;
    g_grid[row][col].text_height = 0;
  }
}

/*
 * sizeAllocate:
 *  - given the current window size and the global grid/font state,
 *    compute column widths, column left offsets, total grid width/height,
 *    whether horizontal/vertical scrollbars are needed, and content sizes.
 *  - returns a SizeAlloc struct; col_widths and col_left are malloc'ed and
 *    must be freed by the caller (free(sa.col_widths); free(sa.col_left);).
 *
 * This centralizes the layout logic so both event handling and drawing use
 * identical numbers.
 */
SizeAlloc sizeAllocate(int win_w, int win_h) {
  SizeAlloc sa;
  sa.col_widths = NULL;
  sa.col_left = NULL;
  sa.need_horz = false;
  sa.need_vert = false;
  sa.total_grid_w = 0.0f;
  sa.total_grid_h = 0.0f;
  sa.content_w = 0.0f;
  sa.content_h = 0.0f;

#ifdef WITH_BORDER
  float border = BORDER_WIDTH;
#else
  float border = 0.0f;
#endif

  float view_x = border;
  float view_y = border;
  float view_w = win_w - 2 * border;
  float view_h = win_h - 2 * border;

  int font_height = TTF_GetFontHeight(g_font);
  float cell_h = font_height + 2 * CELL_PADDING;
  float line_w = GRID_LINE_WIDTH;

  /* total vertical size of grid (all rows + separators) */
  sa.total_grid_h =
      g_rows * cell_h + (g_rows > 0 ? (g_rows - 1) * line_w : 0.0f);

  /* Compute max text width per column */
  int *max_col_w = calloc(g_cols, sizeof(int));
  for (int c = 0; c < g_cols; c++) 
    for (int r = 0; r < g_rows; r++) 
      if (g_grid[r][c].text && g_grid[r][c].text[0] != '\0') 
        if (g_grid[r][c].text_width > max_col_w[c])
          max_col_w[c] = g_grid[r][c].text_width;

  /* Compute column widths */
  sa.col_widths = malloc(g_cols * sizeof(int));
  sa.total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++) {
    sa.col_widths[c] = max_col_w[c] + 2 * CELL_PADDING;
    sa.total_grid_w += sa.col_widths[c];
  }
  sa.total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);

  free(max_col_w);

  /* Interdependency between scrollbars: iteratively compute need_horz/need_vert
   */
  bool need_horz = false;
  bool need_vert = false;
  bool changed = true;
  while (changed) {
    changed = false;
    float temp_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
    float temp_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);
    bool new_horz = sa.total_grid_w > temp_w;
    bool new_vert = sa.total_grid_h > temp_h;
    if (new_horz != need_horz) {
      need_horz = new_horz;
      changed = true;
    }
    if (new_vert != need_vert) {
      need_vert = new_vert;
      changed = true;
    }
  }

  sa.need_horz = need_horz;
  sa.need_vert = need_vert;

  /* content area size after accounting for scrollbars */
  sa.content_w = view_w - (sa.need_vert ? SCROLLBAR_WIDTH : 0.0f);
  sa.content_h = view_h - (sa.need_horz ? SCROLLBAR_WIDTH : 0.0f);

  /* Column left positions (virtual) */
  sa.col_left = malloc(g_cols * sizeof(float));
  if (g_cols > 0)
    sa.col_left[0] = 0.0f;
  for (int c = 1; c < g_cols; c++) {
    sa.col_left[c] = sa.col_left[c - 1] + sa.col_widths[c - 1] + line_w;
  }

  /* store view area for event handlers usage (set in main loop) */
  g_view_x = view_x;
  g_view_y = view_y;
  g_view_w = view_w;
  g_view_h = view_h;

  return sa;
}

/* Draw the grid + scrollbars + texts using values from SizeAlloc.
 * The function does not free sa.col_widths / sa.col_left (caller frees).
 */
void draw_with_alloc(const SizeAlloc *sa) {
  /* Use the global renderer and g_font. Sa contains precomputed sizes. */
  int win_w, win_h;
  SDL_GetWindowSize(g_window, &win_w, &win_h);

#ifdef WITH_BORDER
  float border = BORDER_WIDTH;
#else
  float border = 0.0f;
#endif
  float view_x = g_view_x;
  float view_y = g_view_y;
  float content_w = sa->content_w;
  float content_h = sa->content_h;

  float line_w = GRID_LINE_WIDTH;
  int font_height = TTF_GetFontHeight(g_font);
  float cell_h = font_height + 2 * CELL_PADDING;

  /* Clamp offsets to avoid seeing outside content */
  float max_offset_x = SDL_max(0.0f, sa->total_grid_w - content_w);
  float max_offset_y = SDL_max(0.0f, sa->total_grid_h - content_h);
  g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
  g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

  /* Draw grid background */
  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer,
                     &(SDL_FRect){view_x, view_y, content_w, content_h});

  /* Draw vertical scrollbar if needed */
  float vert_thumb_h = 0.0f, vert_thumb_y = 0.0f;
  if (sa->need_vert) {
    float vert_bar_x = view_x + content_w;
    float vert_bar_y = view_y;
    float vert_bar_h = content_h;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_bar_y,
                                                SCROLLBAR_WIDTH, vert_bar_h});

    vert_thumb_h = SDL_max(10.0f, content_h * (content_h / sa->total_grid_h));

    /* Avoid division by zero if max_offset_y == 0 */
    float max_offset_y = SDL_max(0.0f, sa->total_grid_h - content_h);
    if (max_offset_y <= 0.0f) {
      vert_thumb_y = vert_bar_y;
    } else {
      float track_h = vert_bar_h - vert_thumb_h;
      vert_thumb_y = vert_bar_y + (g_offset_y / max_offset_y) * track_h;
    }

    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_thumb_y,
                                                SCROLLBAR_WIDTH, vert_thumb_h});
  }

  /* Draw horizontal scrollbar if needed */
  float horz_thumb_w = 0.0f, horz_thumb_x = 0.0f;
  if (sa->need_horz) {
    float horz_bar_x = view_x;
    float horz_bar_y = view_y + content_h;
    float horz_bar_w = content_w;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_bar_x, horz_bar_y,
                                                horz_bar_w, SCROLLBAR_WIDTH});

    horz_thumb_w = SDL_max(10.0f, content_w * (content_w / sa->total_grid_w));

    float max_offset_x = SDL_max(0.0f, sa->total_grid_w - content_w);
    if (max_offset_x <= 0.0f) {
      horz_thumb_x = horz_bar_x;
    } else {
      float track_w = horz_bar_w - horz_thumb_w;
      horz_thumb_x = horz_bar_x + (g_offset_x / max_offset_x) * track_w;
    }

    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_thumb_x, horz_bar_y,
                                                horz_thumb_w, SCROLLBAR_WIDTH});
  }

  /* Draw corner if both scrollbars */
  if (sa->need_vert && sa->need_horz) {
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer,
                       &(SDL_FRect){view_x + content_w, view_y + content_h,
                                    SCROLLBAR_WIDTH, SCROLLBAR_WIDTH});
  }

  /* Clip to content area for grid drawing */
  SDL_Rect clip_rect = {(int)view_x, (int)view_y, (int)content_w,
                        (int)content_h};
  SDL_SetRenderClipRect(g_renderer, &clip_rect);

#ifdef WITH_GRID
  /* Collect horizontal grid lines (only visible ones) */
  SDL_FRect *horz_rects = malloc((g_rows - 1) * sizeof(SDL_FRect));
  int horz_count = 0;
  for (int i = 1; i < g_rows; i++) {
    float line_y = view_y - g_offset_y + i * cell_h + (i - 1) * line_w;
    if (line_y >= view_y && line_y <= view_y + content_h) {
      horz_rects[horz_count++] = (SDL_FRect){view_x, line_y, content_w, line_w};
    }
  }

  /* Collect vertical grid lines (only visible ones) */
  SDL_FRect *vert_rects = malloc((g_cols - 1) * sizeof(SDL_FRect));
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float line_x = view_x - g_offset_x + sa->col_left[i];
    if (line_x >= view_x && line_x <= view_x + content_w) {
      vert_rects[vert_count++] = (SDL_FRect){line_x, view_y, line_w, content_h};
    }
  }

  if (horz_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, horz_rects, horz_count);
  }
  if (vert_count > 0) {
    SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
    SDL_RenderFillRects(g_renderer, vert_rects, vert_count);
  }

  free(horz_rects);
  free(vert_rects);
#endif

  /* Draw cell texts (only visible ones) */
  for (int r = 0; r < g_rows; r++) {
    float cell_y = view_y - g_offset_y + r * (cell_h + line_w);
    if (cell_y + cell_h < view_y || cell_y > view_y + content_h)
      continue;

    for (int c = 0; c < g_cols; c++) {
      float cell_x = view_x - g_offset_x + sa->col_left[c];
      if (cell_x + sa->col_widths[c] < view_x || cell_x > view_x + content_w)
        continue;

      if (!g_grid[r][c].text || g_grid[r][c].text[0] == '\0')
        continue;

      size_t len = strlen(g_grid[r][c].text);
      SDL_Surface *label_surface =
          TTF_RenderText_LCD(g_font, g_grid[r][c].text, len, CELL_TEXT_COLOUR,
                             GRID_BACKGROUND_COLOUR);
      if (!label_surface)
        continue;
      SDL_Texture *label_texture =
          SDL_CreateTextureFromSurface(g_renderer, label_surface);
      if (!label_texture) {
        SDL_DestroySurface(label_surface);
        continue;
      }

      float padding_x = 0, padding_y = 0;
      int text_w = label_surface->w;
      int text_h = label_surface->h;

#if CELL_TEXT_POSITION_HORIZONTAL == LEFT
      padding_x = CELL_PADDING;
#elif CELL_TEXT_POSITION_HORIZONTAL == CENTER
      padding_x = (sa->col_widths[c] - text_w) / 2.0f;
#elif CELL_TEXT_POSITION_HORIZONTAL == RIGHT
      padding_x = sa->col_widths[c] - text_w - CELL_PADDING;
#endif

#if CELL_TEXT_POSITION_VERTICAL == TOP
      padding_y = CELL_PADDING;
#elif CELL_TEXT_POSITION_VERTICAL == CENTER
      padding_y = (cell_h - text_h) / 2.0f;
#elif CELL_TEXT_POSITION_VERTICAL == BOTTOM
      padding_y = cell_h - text_h - CELL_PADDING;
#endif

      SDL_RenderTexture(g_renderer, label_texture, NULL,
                        &(SDL_FRect){cell_x + padding_x, cell_y + padding_y,
                                     (float)text_w, (float)text_h});

      SDL_DestroySurface(label_surface);
      SDL_DestroyTexture(label_texture);
    }
  }

  SDL_SetRenderClipRect(g_renderer, NULL);
  SDL_RenderPresent(g_renderer);
}

/* Main program */
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
  DIR *dir = opendir(dir_path);
  if (!dir) {
    fprintf(stderr, "Failed to open directory %s: %s\n", dir_path,
            strerror(errno));
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  int file_count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    file_count++;
  }
  rewinddir(dir);

  /* Set grid dimensions (add 1 row for headers) */
  g_rows = file_count + 1; // +1 for header row
  g_cols = DEFAULT_COLS;   // 4 columns: File, Size (bytes), Date, Permissions
  if (file_count == 0) {
    fprintf(stderr, "No files found in directory %s\n", dir_path);
    closedir(dir);
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
    closedir(dir);
    if (argc == 1)
      free(dir_path);
    return 1;
  }

  /* Allocate grid */
  g_grid = malloc(g_rows * sizeof(Cell *));
  if (!g_grid) {
    fprintf(stderr, "Failed to allocate memory for grid\n");
    closedir(dir);
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
      closedir(dir);
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

  /* Populate grid with file information
   *
   * Important symlink behavior:
   *  - use lstat() to obtain metadata of the directory entry itself
   *    (this gives date + permissions for symlinks)
   *  - if the entry is a symlink, try stat() the same path to get the
   *    target's metadata; take the size from the target. If stat() fails,
   *    set size 0.
   */
  int row = 1; // Start from row 1 to skip headers
  char full_path[PATH_MAX];
  struct stat file_stat;
  while ((entry = readdir(dir)) && row < g_rows) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    /* Use lstat() to read the entry itself (so we can get symlink's
     * mtime/perms) */
    if (lstat(full_path, &file_stat) == -1) {
      fprintf(stderr, "Failed to lstat %s: %s\n", full_path, strerror(errno));
      continue;
    }

    /* Column 0: Filename */
    set_cell(row, 0, entry->d_name);

    /* Column 1: Size in bytes
     * - if this is a symlink, we try to stat() the target and use target size
     * - if stat() fails (target missing), we report size 0
     * - otherwise use file_stat.st_size
     */
    char size_str[32];
    off_t size_value = 0;
    if (S_ISLNK(file_stat.st_mode)) {
      struct stat target_stat;
      if (stat(full_path, &target_stat) == -1) {
        /* target missing or stat failed => size 0 */
        size_value = 0;
      } else {
        size_value = target_stat.st_size;
      }
    } else {
      size_value = file_stat.st_size;
    }
    snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
    set_cell(row, 1, size_str);

    /* Column 2: Date (dd.mm.yyyy)
     * - use the mtime from lstat() so symlink date/mtime is used for symlinks
     */
    char date_str[16];
    struct tm *mtime = localtime(&file_stat.st_mtime);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
    set_cell(row, 2, date_str);

    /* Column 3: Permissions (rwxrwxrwx + setuid, setgid, sticky bit like ls)
     * - build permissions string from file_stat.st_mode (lstat result)
     * - this uses symlink's mode for symlinks, as requested
     */
    char perm_str[11] = "----------";
    mode_t mode = file_stat.st_mode;
    perm_str[0] = (mode & S_IRUSR) ? 'r' : '-';
    perm_str[1] = (mode & S_IWUSR) ? 'w' : '-';
    perm_str[2] = (mode & S_IXUSR) ? ((mode & S_ISUID) ? 's' : 'x')
                                   : ((mode & S_ISUID) ? 'S' : '-');
    perm_str[3] = (mode & S_IRGRP) ? 'r' : '-';
    perm_str[4] = (mode & S_IWGRP) ? 'w' : '-';
    perm_str[5] = (mode & S_IXGRP) ? ((mode & S_ISGID) ? 's' : 'x')
                                   : ((mode & S_ISGID) ? 'S' : '-');
    perm_str[6] = (mode & S_IROTH) ? 'r' : '-';
    perm_str[7] = (mode & S_IWOTH) ? 'w' : '-';
    perm_str[8] = (mode & S_IXOTH) ? ((mode & S_ISVTX) ? 't' : 'x')
                                   : ((mode & S_ISVTX) ? 'T' : '-');
    perm_str[9] = '\0'; // Terminate string
    set_cell(row, 3, perm_str);

    row++;
  }
  closedir(dir);
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
