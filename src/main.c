#include "main.h"
#include <SDL3_ttf/SDL_ttf.h>
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

  SDL_SetRenderClipRect(g_renderer, &(SDL_Rect){x, y, w, h});
  SDL_SetRenderDrawColour(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderFillRect(g_renderer, &(SDL_FRect){x, y, w, h});
#else
  SDL_SetRenderDrawColorSimple(g_renderer, GRID_BACKGROUND_COLOUR);
  SDL_RenderClear(g_renderer);
#endif

#ifdef WITH_GRID
  int count = rows + cols;
  SDL_FRect rects[count];
  int idx = 0;

  float cell_h = (float)h / rows;
  SDL_FRect grid_line_row = {x, y - GRID_LINE_HALF_WIDTH, w, GRID_LINE_WIDTH};

  for (int i = 1; i < rows; i++) {
    grid_line_row.y += cell_h;
    rects[idx++] = grid_line_row;
  }

  float cell_w = (float)w / cols;
  SDL_FRect grid_line_col = {x - GRID_LINE_HALF_WIDTH, y, GRID_LINE_WIDTH, h};

  for (int i = 1; i < cols; i++) {
    grid_line_col.x += cell_w;
    rects[idx++] = grid_line_col;
  }

  SDL_SetRenderDrawColour(g_renderer, GRID_LINE_COLOUR);
  SDL_RenderFillRects(g_renderer, rects, count);
#endif

#ifdef WITH_COLUMN_TITLE
  float cell_x = x;
  int length = COLUMN_TITLE_TEXT_BASE_LENGTH + 1;

  for (int col = 0; col < cols; cell_x += cell_w) {
    char buffer[(col % 10 == 9 ? ++length : length) + 1];
    snprintf(buffer, sizeof(buffer), COLUMN_TITLE_TEXT_LABEL, ++col);

    SDL_Surface *labelSurface =
        TTF_RenderText_LCD(g_font, buffer, length, COLUMN_TITLE_TEXT_COLOUR,
                           GRID_BACKGROUND_COLOUR);

    SDL_Texture *labelTexture =
        SDL_CreateTextureFromSurface(g_renderer, labelSurface);
    SDL_DestroySurface(labelSurface);

    SDL_RenderTexture(g_renderer, labelTexture, nullptr,
                      &(SDL_FRect){cell_x + GRID_LINE_WIDTH,
                                   y + GRID_LINE_WIDTH,
                                   cell_w - 2 * GRID_LINE_WIDTH,
                                   cell_h - 2 * GRID_LINE_WIDTH});
    SDL_DestroyTexture(labelTexture);
  }
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
  SDL_CHECK(TTF_Init(), "SDL-ttf initialisation failed");

#ifdef WITH_COLUMN_TITLE
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