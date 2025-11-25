#include "include/columns.h"
#include "include/config.h"
#include "include/fileentry.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

ColumnRegistry *cols_create(void) {
  ColumnRegistry *reg = malloc(sizeof *reg);
  if (!reg)
    return NULL;

  reg->columns = NULL;
  reg->count = 0;
  reg->capacity = 0;
  return reg;
}

void cols_add(ColumnRegistry *reg, ColumnDef col) {
  if (!reg)
    return;

  if (reg->count >= reg->capacity) {
    int new_cap = reg->capacity == 0 ? 8 : reg->capacity * 2;
    ColumnDef *new_cols =
        realloc(reg->columns, (size_t)new_cap * sizeof *new_cols);
    if (!new_cols)
      return;
    reg->columns = new_cols;
    reg->capacity = new_cap;
  }

  reg->columns[reg->count] = col;
  reg->count++;
}

void cols_insert(ColumnRegistry *reg, int col_idx, ColumnDef col) {
  if (!reg || col_idx < 0 || col_idx > reg->count)
    return;

  if (reg->count >= reg->capacity) {
    int new_cap = reg->capacity == 0 ? 8 : reg->capacity * 2;
    ColumnDef *new_cols =
        realloc(reg->columns, (size_t)new_cap * sizeof *new_cols);
    if (!new_cols)
      return;
    reg->columns = new_cols;
    reg->capacity = new_cap;
  }

  /* Shift columns to the right */
  for (int i = reg->count; i > col_idx; i--) {
    reg->columns[i] = reg->columns[i - 1];
  }

  reg->columns[col_idx] = col;
  reg->count++;
}

void cols_remove(ColumnRegistry *reg, int col_idx) {
  if (!reg || col_idx < 0 || col_idx >= reg->count)
    return;

  for (int i = col_idx; i < reg->count - 1; i++) {
    reg->columns[i] = reg->columns[i + 1];
  }
  reg->count--;
}

void cols_move(ColumnRegistry *reg, int src_idx, int dst_idx) {
  if (!reg || src_idx < 0 || src_idx >= reg->count || dst_idx < 0 ||
      dst_idx >= reg->count || src_idx == dst_idx)
    return;

  ColumnDef temp = reg->columns[src_idx];

  if (src_idx < dst_idx) {
    for (int i = src_idx; i < dst_idx; i++) {
      reg->columns[i] = reg->columns[i + 1];
    }
  } else {
    for (int i = src_idx; i > dst_idx; i--) {
      reg->columns[i] = reg->columns[i - 1];
    }
  }

  reg->columns[dst_idx] = temp;
}

ColumnDef *cols_get(ColumnRegistry *reg, int col_idx) {
  if (!reg || col_idx < 0 || col_idx >= reg->count)
    return NULL;
  return &reg->columns[col_idx];
}

void cols_destroy(ColumnRegistry *reg) {
  if (!reg)
    return;
  free(reg->columns);
  free(reg);
}

/* --- Predefined columns --- */

static char *render_path_cell(void *user_data, void *row_data) {
  const ColumnDef *col = (const ColumnDef *)user_data;
  const FileEntry *entry = (const FileEntry *)row_data;

  if (!entry)
    return strdup("");

  char buf[PATH_MAX * 2] = {0};

  const char *tmpl = col->cell_template ? col->cell_template : "%n";
  const char *src = tmpl;
  char *dst = buf;
  size_t remain = sizeof buf - 1;

  while (*src && remain > 1) {
    if (*src == '%' && src[1]) {
      src++;
      const char *replace = NULL;

      switch (*src) {
      case '%':
        replace = "%";
        break;
      case 'n':
        replace = entry->name;
        break;
      case 'f':
        replace = entry->full_path;
        break;
      case 'F':
        replace =
            entry->resolved_path ? entry->resolved_path : entry->full_path;
        break;
      case 'd':
        replace = entry->dir_path;
        break;
      case 'r':
        replace = entry->root_path;
        break;
      case 'P':
        replace = entry->root_path ? entry->root_path : ".";
        break;
      default:
        if (remain >= 2) {
          *dst++ = '%';
          *dst++ = *src;
          remain -= 2;
        }
        break;
      }

      if (replace) {
        size_t len = strlen(replace);
        if (len > remain)
          len = remain;
        memcpy(dst, replace, len);
        dst += len;
        remain -= len;
      }
      src++;
    } else {
      *dst++ = *src++;
      remain--;
    }
  }
  *dst = '\0';

  return strdup(buf);
}

static char *render_size_cell(void *user_data, void *row_data) {
  (void)user_data;
  const FileEntry *entry = (const FileEntry *)row_data;

  if (!entry)
    return strdup("");

  char buf[64];
  snprintf(buf, sizeof buf, "%lld", (long long)entry->st.st_size);
  return strdup(buf);
}

static char *render_date_cell(void *user_data, void *row_data) {
  const ColumnDef *col = (const ColumnDef *)user_data;
  const FileEntry *entry = (const FileEntry *)row_data;

  if (!entry)
    return strdup("");

  char buf[128];
  struct tm tm_buf;
  const char *fmt = col->cell_template && col->cell_template[0]
                        ? col->cell_template
                        : DATE_FORMAT_TEMPLATE;

  if (localtime_r(&entry->st.st_mtime, &tm_buf)) {
    strftime(buf, sizeof buf, fmt, &tm_buf);
  } else {
    strcpy(buf, "???");
  }

  return strdup(buf);
}

static char *render_perms_cell(void *user_data, void *row_data) {
  (void)user_data;
  const FileEntry *entry = (const FileEntry *)row_data;

  if (!entry)
    return strdup("");

  mode_t m = entry->st.st_mode;
  char buf[64];
  snprintf(buf, sizeof buf, "%c%c%c%c%c%c%c%c%c%c",
           S_ISDIR(m)   ? 'd'
           : S_ISLNK(m) ? 'l'
           : S_ISREG(m) ? '-'
                        : '?',
           (m & S_IRUSR) ? 'r' : '-', (m & S_IWUSR) ? 'w' : '-',
           (m & S_IXUSR) ? 'x' : '-', (m & S_IRGRP) ? 'r' : '-',
           (m & S_IWGRP) ? 'w' : '-', (m & S_IXGRP) ? 'x' : '-',
           (m & S_IROTH) ? 'r' : '-', (m & S_IWOTH) ? 'w' : '-',
           (m & S_IXOTH) ? 'x' : '-');
  return strdup(buf);
}

ColumnDef col_path_default(void) {
  return (ColumnDef){
      .type = COL_PATH,
      .cell_template = "%n",
      .header_template = HEADER_TEMPLATE_0,
      .width_min = 50,
      .width_max = 500,
      .user_data = NULL,
      .render_cell = render_path_cell,
      .render_header = NULL,
  };
}

ColumnDef col_size_default(void) {
  return (ColumnDef){
      .type = COL_SIZE,
      .cell_template = NULL,
      .header_template = HEADER_TEMPLATE_1,
      .width_min = 40,
      .width_max = 150,
      .user_data = NULL,
      .render_cell = render_size_cell,
      .render_header = NULL,
  };
}

ColumnDef col_date_default(void) {
  return (ColumnDef){
      .type = COL_DATE,
      .cell_template = DATE_FORMAT_TEMPLATE,
      .header_template = HEADER_TEMPLATE_2,
      .width_min = 60,
      .width_max = 300,
      .user_data = NULL,
      .render_cell = render_date_cell,
      .render_header = NULL,
  };
}

ColumnDef col_perms_default(void) {
  return (ColumnDef){
      .type = COL_PERMS,
      .cell_template = PERM_TEMPLATE,
      .header_template = HEADER_TEMPLATE_3,
      .width_min = 60,
      .width_max = 200,
      .user_data = NULL,
      .render_cell = render_perms_cell,
      .render_header = NULL,
  };
}