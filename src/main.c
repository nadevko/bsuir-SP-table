#include "main.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a) ==
         0;
}

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

void draw(void) {
  int win_w, win_h;
  SDL_GetWindowSize(g_window, &win_w, &win_h);

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

  float total_grid_h =
      g_rows * cell_h + (g_rows > 0 ? (g_rows - 1) * line_w : 0.0f);

  // Compute max text width per column
  int *max_col_w = calloc(g_cols, sizeof(int));
  for (int c = 0; c < g_cols; c++) {
    for (int r = 0; r < g_rows; r++) {
      if (g_grid[r][c].text && g_grid[r][c].text[0] != '\0') {
        max_col_w[c] = SDL_max(max_col_w[c], g_grid[r][c].text_width);
      }
    }
  }

  // Compute column widths
  int *col_widths = malloc(g_cols * sizeof(int));
  float total_grid_w = 0.0f;
  for (int c = 0; c < g_cols; c++) {
    col_widths[c] = max_col_w[c] + 2 * CELL_PADDING;
    total_grid_w += col_widths[c];
  }
  total_grid_w += (g_cols > 0 ? (g_cols - 1) * line_w : 0.0f);
  free(max_col_w);

  // Determine if scrollbars are needed (handle interdependency)
  bool need_horz = false;
  bool need_vert = false;
  bool changed = true;
  while (changed) {
    changed = false;
    float temp_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
    float temp_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);
    bool new_horz = total_grid_w > temp_w;
    bool new_vert = total_grid_h > temp_h;
    if (new_horz != need_horz) {
      need_horz = new_horz;
      changed = true;
    }
    if (new_vert != need_vert) {
      need_vert = new_vert;
      changed = true;
    }
  }

  float content_w = view_w - (need_vert ? SCROLLBAR_WIDTH : 0.0f);
  float content_h = view_h - (need_horz ? SCROLLBAR_WIDTH : 0.0f);

  // Clamp offsets
  float max_offset_x = SDL_max(0.0f, total_grid_w - content_w);
  float max_offset_y = SDL_max(0.0f, total_grid_h - content_h);
  g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_offset_x);
  g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_offset_y);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

  // Draw grid background
  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer,
                     &(SDL_FRect){view_x, view_y, content_w, content_h});

  // Draw vertical scrollbar if needed
  if (need_vert) {
    float vert_bar_x = view_x + content_w;
    float vert_bar_y = view_y;
    float vert_bar_h = content_h;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_bar_y,
                                                SCROLLBAR_WIDTH, vert_bar_h});

    float thumb_h = SDL_max(10.0f, content_h * (content_h / total_grid_h));
    float thumb_y =
        vert_bar_y + (g_offset_y / max_offset_y) * (vert_bar_h - thumb_h);
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, thumb_y,
                                                SCROLLBAR_WIDTH, thumb_h});
  }

  // Draw horizontal scrollbar if needed
  if (need_horz) {
    float horz_bar_x = view_x;
    float horz_bar_y = view_y + content_h;
    float horz_bar_w = content_w;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_bar_x, horz_bar_y,
                                                horz_bar_w, SCROLLBAR_WIDTH});

    float thumb_w = SDL_max(10.0f, content_w * (content_w / total_grid_w));
    float thumb_x =
        horz_bar_x + (g_offset_x / max_offset_x) * (horz_bar_w - thumb_w);
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){thumb_x, horz_bar_y, thumb_w,
                                                SCROLLBAR_WIDTH});
  }

  // Draw corner if both scrollbars
  if (need_vert && need_horz) {
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer,
                       &(SDL_FRect){view_x + content_w, view_y + content_h,
                                    SCROLLBAR_WIDTH, SCROLLBAR_WIDTH});
  }

  // Clip to content area for grid drawing
  SDL_Rect clip_rect = {(int)view_x, (int)view_y, (int)content_w,
                        (int)content_h};
  SDL_SetRenderClipRect(g_renderer, &clip_rect);

  // Compute column left positions (virtual)
  float *col_left = malloc(g_cols * sizeof(float));
  if (g_cols > 0)
    col_left[0] = 0.0f;
  for (int c = 1; c < g_cols; c++) {
    col_left[c] = col_left[c - 1] + col_widths[c - 1] + line_w;
  }

#ifdef WITH_GRID
  // Collect horizontal grid lines (only visible ones)
  SDL_FRect *horz_rects = malloc((g_rows - 1) * sizeof(SDL_FRect));
  int horz_count = 0;
  for (int i = 1; i < g_rows; i++) {
    float line_y = view_y - g_offset_y + i * cell_h + (i - 1) * line_w;
    if (line_y >= view_y && line_y <= view_y + content_h) {
      horz_rects[horz_count++] = (SDL_FRect){view_x, line_y, content_w, line_w};
    }
  }

  // Collect vertical grid lines (only visible ones)
  SDL_FRect *vert_rects = malloc((g_cols - 1) * sizeof(SDL_FRect));
  int vert_count = 0;
  for (int i = 1; i < g_cols; i++) {
    float line_x = view_x - g_offset_x + col_left[i];
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

  // Draw cell texts (only visible ones)
  for (int r = 0; r < g_rows; r++) {
    float cell_y = view_y - g_offset_y + r * (cell_h + line_w);
    if (cell_y + cell_h < view_y || cell_y > view_y + content_h)
      continue;

    for (int c = 0; c < g_cols; c++) {
      float cell_x = view_x - g_offset_x + col_left[c];
      if (cell_x + col_widths[c] < view_x || cell_x > view_x + content_w)
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
      padding_x = (col_widths[c] - text_w) / 2.0f;
#elif CELL_TEXT_POSITION_HORIZONTAL == RIGHT
      padding_x = col_widths[c] - text_w - CELL_PADDING;
#endif

#if CELL_TEXT_POSITION_VERTICAL == TOP
      padding_y = CELL_PADDING;
#elif CELL_TEXT_POSITION_VERTICAL == VCENTER
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

  free(col_left);
  free(col_widths);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    g_rows = atoi(argv[1]);
    g_cols = atoi(argv[2]);
    if (g_rows <= 0 || g_cols <= 0) {
      fprintf(stderr, "Columns and rows must be positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
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
    return 1;
  }

  // Allocate grid
  g_grid = malloc(g_rows * sizeof(Cell *));
  for (int r = 0; r < g_rows; r++) {
    g_grid[r] = calloc(g_cols, sizeof(Cell));
  }

  // Set example top row using new function
  for (int col = 0; col < g_cols; col++) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Column %i", col + 1);
    set_cell(0, col, buffer);
  }

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  bool running = true;
  SDL_Event event;
  bool need_horz = false;
  bool need_vert = false;
  float total_grid_w = 0.0f;
  float total_grid_h = 0.0f;

  while (running) {
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
        g_offset_y +=
            (NATURAL_SCROLL ? SCROLL_SPEED
                            : -SCROLL_SPEED); // Up: increase offset (natural),
                                              // decrease (traditional)
        break;
      case SDLK_DOWN:
        g_offset_y +=
            (NATURAL_SCROLL ? -SCROLL_SPEED
                            : SCROLL_SPEED); // Down: decrease offset (natural),
                                             // increase (traditional)
        break;
      case SDLK_LEFT:
        g_offset_x += (NATURAL_SCROLL
                           ? SCROLL_SPEED
                           : -SCROLL_SPEED); // Left: increase offset (natural),
                                             // decrease (traditional)
        break;
      case SDLK_RIGHT:
        g_offset_x += (NATURAL_SCROLL
                           ? -SCROLL_SPEED
                           : SCROLL_SPEED); // Right: decrease offset (natural),
                                            // increase (traditional)
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
        int win_w_local, win_h_local;
        SDL_GetWindowSize(g_window, &win_w_local, &win_h_local);

#ifdef WITH_BORDER
        float border_local = BORDER_WIDTH;
#else
        float border_local = 0.0f;
#endif
        float view_w_local = win_w_local - 2 * border_local;
        float view_h_local = win_h_local - 2 * border_local;
        float content_w_local =
            view_w_local - (need_vert ? SCROLLBAR_WIDTH : 0);
        float content_h_local =
            view_h_local - (need_horz ? SCROLLBAR_WIDTH : 0);

        // Vertical scrollbar
        if (need_vert) {
          float vert_bar_x = border_local + content_w_local;
          float vert_bar_y = border_local;
          float vert_bar_h = content_h_local;
          if (mx >= vert_bar_x && mx < vert_bar_x + SCROLLBAR_WIDTH &&
              my >= vert_bar_y && my < vert_bar_y + vert_bar_h) {
            g_dragging_vert = true;
            g_drag_start_pos = my;
            g_drag_start_offset = g_offset_y;
          }
        }

        // Horizontal scrollbar
        if (need_horz) {
          float horz_bar_x = border_local;
          float horz_bar_y = border_local + content_h_local;
          float horz_bar_w = content_w_local;
          if (mx >= horz_bar_x && mx < horz_bar_x + horz_bar_w &&
              my >= horz_bar_y && my < horz_bar_y + SCROLLBAR_WIDTH) {
            g_dragging_horz = true;
            g_drag_start_pos = mx;
            g_drag_start_offset = g_offset_x;
          }
        }
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (g_dragging_vert) {
        int win_h_local;
        SDL_GetWindowSize(g_window, NULL, &win_h_local);
#ifdef WITH_BORDER
        float border_local = BORDER_WIDTH;
#else
        float border_local = 0.0f;
#endif
        float view_h_local = win_h_local - 2 * border_local;
        float content_h_local =
            view_h_local - (need_horz ? SCROLLBAR_WIDTH : 0);
        float dy = event.motion.yrel;
        float scroll_factor = NATURAL_SCROLL ? -1.0f : 1.0f;
        float new_offset_y = g_drag_start_offset + scroll_factor * dy;
        g_offset_y =
            SDL_clamp(new_offset_y, 0.0f, total_grid_h - content_h_local);
      } else if (g_dragging_horz) {
        int win_w_local;
        SDL_GetWindowSize(g_window, &win_w_local, NULL);
#ifdef WITH_BORDER
        float border_local = BORDER_WIDTH;
#else
        float border_local = 0.0f;
#endif
        float view_w_local = win_w_local - 2 * border_local;
        float content_w_local =
            view_w_local - (need_vert ? SCROLLBAR_WIDTH : 0);
        float dx = event.motion.xrel;
        float scroll_factor = NATURAL_SCROLL ? -1.0f : 1.0f;
        float new_offset_x = g_drag_start_offset + scroll_factor * dx;
        g_offset_x =
            SDL_clamp(new_offset_x, 0.0f, total_grid_w - content_w_local);
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        g_dragging_vert = false;
        g_dragging_horz = false;
      }
      break;
    }

    if (running) {
      draw();
    }
  }

  return 0;
}