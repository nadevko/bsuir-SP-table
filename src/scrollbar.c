// src/scrollbar.c
#include "include/scrollbar.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/utils.h"
#include <SDL3/SDL.h>

/* Рисует ползунки и угол (если оба ползунка видны). Повторяет прежнюю логику,
   но вынесена из grid.c для разделения ответственности. */
void draw_scrollbars(const SizeAlloc *sa) {
  float view_x = g_view_x;
  float view_y = g_view_y;
  float content_w = sa->content_w;
  float content_h = sa->content_h;
  float vert_thumb_h = 0.0f, vert_thumb_y = 0.0f;
  float horz_thumb_w = 0.0f, horz_thumb_x = 0.0f;

  /* Vertical scrollbar */
  if (sa->need_vert) {
    float vert_bar_x = view_x + content_w;
    float vert_bar_y = view_y;
    float vert_bar_h = content_h;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_bar_y,
                                                SCROLLBAR_WIDTH, vert_bar_h});

    /* thumb size & position */
    vert_thumb_h = SDL_max(10.0f, content_h * (content_h / sa->total_grid_h));
    float max_offset_y_local = SDL_max(0.0f, sa->total_grid_h - content_h);
    if (max_offset_y_local <= 0.0f) {
      vert_thumb_y = vert_bar_y;
    } else {
      float track_h = vert_bar_h - vert_thumb_h;
      vert_thumb_y = vert_bar_y + (g_offset_y / max_offset_y_local) * track_h;
    }
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){vert_bar_x, vert_thumb_y,
                                                SCROLLBAR_WIDTH, vert_thumb_h});
  }

  /* Horizontal scrollbar */
  if (sa->need_horz) {
    float horz_bar_x = view_x;
    float horz_bar_y = view_y + content_h;
    float horz_bar_w = content_w;
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_bar_x, horz_bar_y,
                                                horz_bar_w, SCROLLBAR_WIDTH});

    horz_thumb_w = SDL_max(10.0f, content_w * (content_w / sa->total_grid_w));
    float max_offset_x_local = SDL_max(0.0f, sa->total_grid_w - content_w);
    if (max_offset_x_local <= 0.0f) {
      horz_thumb_x = horz_bar_x;
    } else {
      float track_w = horz_bar_w - horz_thumb_w;
      horz_thumb_x = horz_bar_x + (g_offset_x / max_offset_x_local) * track_w;
    }
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_THUMB_COLOUR);
    SDL_RenderFillRect(g_renderer, &(SDL_FRect){horz_thumb_x, horz_bar_y,
                                                horz_thumb_w, SCROLLBAR_WIDTH});
  }

  /* Corner if both visible */
  if (sa->need_vert && sa->need_horz) {
    SDL_SetRenderDrawColour(g_renderer, SCROLLBAR_BG_COLOUR);
    SDL_RenderFillRect(g_renderer,
                       &(SDL_FRect){view_x + content_w, view_y + content_h,
                                    SCROLLBAR_WIDTH, SCROLLBAR_WIDTH});
  }
}
