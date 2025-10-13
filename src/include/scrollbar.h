// include/scrollbar.h
#pragma once
#include "types.h"

/* Рисует вертикальную/горизонтальную полосу прокрутки и угол, если нужно.
   Использует глобальные значения
   (g_view_x/g_view_y/g_content_w/g_content_h/g_offset_* и т.д.) sa требуется
   для размеров/флагов need_horz/need_vert */
void draw_scrollbars(const SizeAlloc *sa);
