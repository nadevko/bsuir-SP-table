#include "main.h"
#include <stdio.h>
#include <stdlib.h>

void cleanup(void) {
  if (g_font != nullptr) {
    TTF_CloseFont(g_font);
    g_font = nullptr;
  }
  if (g_renderer != nullptr) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window != nullptr) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
  SDL_Quit();
}

bool SDL_SetRenderDrawColour(SDL_Renderer *renderer, SDL_Color color) {
  return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw() {
  int x = 0, y = 0, w, h;

  SDL_GetWindowSize(g_window, &w, &h);

#ifdef WITH_BORDER
  SDL_SetRenderDrawColour(g_renderer, BORDER_COLOUR);
  SDL_RenderClear(g_renderer);

  x += BORDER_WIDTH;
  y += BORDER_WIDTH;
  w -= x + BORDER_WIDTH;
  h -= y + BORDER_WIDTH;

  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer, &(SDL_FRect){x, y, w, h});
#else
  SDL_SetRenderDrawColorSimple(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

  float cell_h = (h - (rows - 1) * GRID_LINE_WIDTH) / rows;
  float cell_w = (w - (cols - 1) * GRID_LINE_WIDTH) / cols;

#ifdef WITH_COLUMN_TITLE
  float cell_x = x;
  [[gnu::const]] auto base_length = strlen(COLUMN_TITLE_TEXT_LABEL) - 1;
  int length = base_length;

  for (int col = 1; col <= cols; col++) {
    float padding_x = 0, padding_y = 0;
    char buffer[(col % 10 == 0 ? ++length : length) + 1];
    snprintf(buffer, sizeof(buffer), COLUMN_TITLE_TEXT_LABEL, col);

    SDL_Surface *label_surface =
        TTF_RenderText_LCD(g_font, buffer, length, COLUMN_TITLE_TEXT_COLOUR,
                           GRID_BACKGROUND_COLOUR);
    SDL_Texture *label_texture =
        SDL_CreateTextureFromSurface(g_renderer, label_surface);

#if COLUMN_TITLE_TEXT_POSITION_HORIZONTAL == CENTER
    padding_x = (cell_w - label_surface->w) / 2;
#elif COLUMN_TITLE_TEXT_POSITION_HORIZONTAL == RIGHT
    padding_x = cell_w - labelSurface->w;
#endif
#if COLUMN_TITLE_TEXT_POSITION_VERTICAL == CENTER
    padding_y = (cell_h - label_surface->h) / 2;
#elif COLUMN_TITLE_TEXT_POSITION_VERTICAL == BOTTOM
    padding_y = cell_h - labelSurface->h;
#endif

    SDL_SetRenderClipRect(g_renderer, &(SDL_Rect){cell_x, y, cell_w, cell_h});
    SDL_RenderTexture(g_renderer, label_texture, nullptr,
                      &(SDL_FRect){cell_x + padding_x, y + padding_y,
                                   label_surface->w, label_surface->h});

    SDL_DestroySurface(label_surface);
    SDL_DestroyTexture(label_texture);
    cell_x += cell_w + GRID_LINE_WIDTH;
  }
#endif

#ifdef WITH_GRID
  SDL_SetRenderClipRect(g_renderer, &(SDL_Rect){x, y, w, h});
  SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);

  int count = rows + cols - 2;
  SDL_FRect rects[count];
  int idx = 0;

  SDL_FRect grid_line_row = {x, y + cell_h, w, GRID_LINE_WIDTH};

  for (int row = 1; row < rows; row++) {
    rects[idx++] = grid_line_row;
    grid_line_row.y += cell_h + GRID_LINE_WIDTH;
  }

  SDL_FRect grid_line_col = {x + cell_w, y, GRID_LINE_WIDTH, h};

  for (int col = 1; col < cols; col++) {
    rects[idx++] = grid_line_col;
    grid_line_col.x += cell_w + GRID_LINE_WIDTH;
  }

  SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
  SDL_RenderFillRects(g_renderer, rects, count);
#endif

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    rows = atoi(argv[1]);
    cols = atoi(argv[2]);
    if (rows <= 0 || cols <= 0) {
      fprintf(stderr, "Columns and rows must be positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  atexit(cleanup);

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed");

#ifdef WITH_COLUMN_TITLE
  SDL_CHECK(TTF_Init(), "SDL-ttf initialisation failed");
#ifdef WITH_FONTCONFIG
  FcConfig *fontconfig;

  ANY_CHECK(fontconfig = FcInitLoadConfigAndFonts(),
            "FcConfig initialisation failed");

  FcPattern *pattern;
  ANY_CHECK(pattern = FcNameParse((const FcChar8 *)COLUMN_TITLE_TEXT_NAME),
            "Failed to parse font name");

  FcConfigSubstitute(fontconfig, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match;
  ANY_CHECK(match = FcFontMatch(fontconfig, pattern, &result),
            "Failed to resolve font");

  FcChar8 *file = nullptr;
  ANY_CHECK(FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch,
            "Failed to find font file");

  g_font = TTF_OpenFont((const char *)file, COLUMN_TITLE_TEXT_SIZE);

  FcPatternDestroy(pattern);
  FcPatternDestroy(match);
  FcConfigDestroy(fontconfig);
#else
  g_font = TTF_OpenFont(COLUMN_TITLE_TEXT_NAME, COLUMN_TITLE_TEXT_SIZE);
#endif
#endif

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_FULLSCREEN |
                                            SDL_WINDOW_BORDERLESS,
                                        &g_window, &g_renderer),
            "Window and renderer creation failed");

  bool running = true;
  SDL_Event event;

  while (running) {
    draw();
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
      }
      break;
    }
  }

  return 0;
}