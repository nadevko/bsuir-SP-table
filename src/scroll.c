#include "include/scroll.h"
#include "include/config.h"
#include "include/globals.h"
#include <SDL3/SDL.h>
#include <math.h>

/* Вспомогательная: клампинг оффсетов и целей в допустимые границы */
static void clamp_all_internal(void) {
  float max_off_x = SDL_max(0.0f, g_total_grid_w - g_content_w);
  float max_off_y = SDL_max(0.0f, g_total_grid_h - g_content_h);

  g_offset_x = SDL_clamp(g_offset_x, 0.0f, max_off_x);
  g_offset_y = SDL_clamp(g_offset_y, 0.0f, max_off_y);

  g_scroll_target_x = SDL_clamp(g_scroll_target_x, 0.0f, max_off_x);
  g_scroll_target_y = SDL_clamp(g_scroll_target_y, 0.0f, max_off_y);
}

/* Добавить дельту к цели прокрутки (обычно используется при SMOOTH_SCROLL == 1)
 */
void scroll_add_target(float dx, float dy) {
  g_scroll_target_x += dx;
  g_scroll_target_y += dy;
  clamp_all_internal();
}

/* Немедленно применить дельту к видимому offset и синхронизировать цель.
   Используется когда нужен мгновенный эффект (SMOOTH_SCROLL == 0)
   или когда пользователь тянет ползунок. */
void scroll_apply_immediate(float dx, float dy) {
  g_offset_x += dx;
  g_offset_y += dy;
  g_scroll_target_x = g_offset_x;
  g_scroll_target_y = g_offset_y;
  clamp_all_internal();
}

/* Вынесенная публичная возможность — принудительно отконстрейнть значения. */
void scroll_clamp_all(void) { clamp_all_internal(); }

void update_scroll(void) {
#ifdef SMOOTH_SCROLL
  /* Параметры анимации — можно подстроить под вкусы/платформу */
  const float scroll_anim_factor = 0.22f; /* доля пути за кадр (0..1) */
  const float min_scroll_step =
      0.5f; /* минимальная разница для продолжения анимации */

  /* Вертикальная анимация (если не тянут вертикальный ползунок) */
  if (!g_dragging_vert) {
    float dy = g_scroll_target_y - g_offset_y;
    if (fabsf(dy) > min_scroll_step) {
      g_offset_y += dy * scroll_anim_factor;
    } else {
      g_offset_y = g_scroll_target_y;
    }
  } else {
    /* Если пользователь тянет — держим цель синхронизованной с текущим offset
     */
    g_scroll_target_y = g_offset_y;
  }

  /* Горизонтальная анимация */
  if (!g_dragging_horz) {
    float dx = g_scroll_target_x - g_offset_x;
    if (fabsf(dx) > min_scroll_step) {
      g_offset_x += dx * scroll_anim_factor;
    } else {
      g_offset_x = g_scroll_target_x;
    }
  } else {
    g_scroll_target_x = g_offset_x;
  }
#else
  /* Если плавность выключена — offsets должны уже управляться напрямую
     событиями. Тем не менее синхронизируем offset с целью на всякий случай. */
  g_offset_x = g_scroll_target_x;
  g_offset_y = g_scroll_target_y;
#endif

  /* В конце — гарантируем корректные границы (устраняем возможный drift) */
  clamp_all_internal();
}