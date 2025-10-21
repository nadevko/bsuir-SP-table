#include "include/fs.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/utils.h"
#include "include/virtual_scroll.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  char *display_name;
  char *size_str;
  char *date_str;
  char *perm_str;
} FileInfo;

static FileInfo batch[BATCH_SIZE];
static int batch_count = 0;

static void flush_batch(void) {
  if (batch_count == 0)
    return;

  if (g_stop)
    return;

  SDL_LockMutex(g_grid_mutex);

  int old_rows = g_rows;
  g_rows += batch_count;
  g_grid = realloc(g_grid, g_rows * sizeof(Cell *));
  if (!g_grid) {
    log_fs_error("Failed to realloc grid");
    SDL_UnlockMutex(g_grid_mutex);
    return;
  }

  for (int i = 0; i < batch_count; i++) {
    int r = old_rows + i;
    g_grid[r] = calloc(g_cols, sizeof(Cell));
    if (!g_grid[r]) {
      log_fs_error("Failed to alloc grid row %d", r);
      continue;
    }

    set_cell(r, 0, batch[i].display_name);
    set_cell(r, 1, batch[i].size_str);
    set_cell(r, 2, batch[i].date_str);
    set_cell(r, 3, batch[i].perm_str);

    for (int c = 0; c < g_cols; c++) {
      g_max_col_widths[c] =
          SDL_max(g_max_col_widths[c], g_grid[r][c].text_width);
    }

    free(batch[i].display_name);
    free(batch[i].size_str);
    free(batch[i].date_str);
    free(batch[i].perm_str);
  }

  if (g_vscroll) {
    g_vscroll->total_virtual_rows = g_rows;
  }

  SDL_UnlockMutex(g_grid_mutex);

  batch_count = 0;
}

static void add_file(const char *display_name, const char *size_str,
                     const char *date_str, const char *perm_str) {
  if (batch_count == BATCH_SIZE) {
    flush_batch();
  }

  batch[batch_count].display_name = strdup(display_name);
  batch[batch_count].size_str = strdup(size_str);
  batch[batch_count].date_str = strdup(date_str);
  batch[batch_count].perm_str = strdup(perm_str);
  batch_count++;
}

static void format_size(off_t size, char *buf, size_t len) {
  snprintf(buf, len, "%lld", (long long)size);
}

static void format_date(time_t mtime, char *buf, size_t len) {
  struct tm *tm = localtime(&mtime);
  strftime(buf, len, "%d.%m.%Y", tm);
}

static void format_perms(mode_t mode, char *buf, size_t len) {
#if PERM_FORMAT == PERM_NUMERIC
  snprintf(buf, len, "%04o", (unsigned)(mode & 07777));
#else
  int idx = 0;
  char user[4], group[4], other[4];

  user[0] = (mode & S_IRUSR) ? 'r' : '-';
  user[1] = (mode & S_IWUSR) ? 'w' : '-';
  user[2] = (mode & S_IXUSR) ? 'x' : '-';
  user[3] = '\0';

  group[0] = (mode & S_IRGRP) ? 'r' : '-';
  group[1] = (mode & S_IWGRP) ? 'w' : '-';
  group[2] = (mode & S_IXGRP) ? 'x' : '-';
  group[3] = '\0';

  other[0] = (mode & S_IROTH) ? 'r' : '-';
  other[1] = (mode & S_IWOTH) ? 'w' : '-';
  other[2] = (mode & S_IXOTH) ? 'x' : '-';
  other[3] = '\0';

  char extra[4] = {'-', '-', '-', '\0'};
  if (mode & S_ISUID)
    extra[0] = (mode & S_IXUSR) ? 's' : 'S';
  if (mode & S_ISGID)
    extra[1] = (mode & S_IXGRP) ? 's' : 'S';
  if (mode & S_ISVTX)
    extra[2] = (mode & S_IXOTH) ? 't' : 'T';

#if SHORTENED_EXTRA_BITS
  if (mode & S_ISUID)
    user[2] = (mode & S_IXUSR) ? 's' : 'S';
  if (mode & S_ISGID)
    group[2] = (mode & S_IXGRP) ? 's' : 'S';
  if (mode & S_ISVTX)
    other[2] = (mode & S_IXOTH) ? 't' : 'T';
#endif

#if !SHORTENED_EXTRA_BITS
  buf[idx++] = extra[0];
  buf[idx++] = extra[1];
  buf[idx++] = extra[2];
#if ADD_PERMISSIONS_WHITESPACES
  buf[idx++] = ' ';
#endif
#endif

  if (SHOW_FILE_TYPE) {
    if (S_ISDIR(mode))
      buf[idx++] = 'd';
    else if (S_ISLNK(mode))
      buf[idx++] = 'l';
    else if (S_ISREG(mode))
      buf[idx++] = '-';
    else if (S_ISCHR(mode))
      buf[idx++] = 'c';
    else if (S_ISBLK(mode))
      buf[idx++] = 'b';
    else if (S_ISFIFO(mode))
      buf[idx++] = 'p';
    else if (S_ISSOCK(mode))
      buf[idx++] = 's';
    else
      buf[idx++] = '?';
  }

  for (int i = 0; i < 3; ++i)
    buf[idx++] = user[i];

#if ADD_PERMISSIONS_WHITESPACES
  buf[idx++] = ' ';
#endif

  for (int i = 0; i < 3; ++i)
    buf[idx++] = group[i];

#if ADD_PERMISSIONS_WHITESPACES
  buf[idx++] = ' ';
#endif

  for (int i = 0; i < 3; ++i)
    buf[idx++] = other[i];

  buf[idx] = '\0';
#endif
}

static void traverse_recursive(const char *dir_path, const char *prefix,
                               int depth) {
  if (g_stop)
    return;

  DIR *dir = opendir(dir_path);
  if (!dir) {
    log_fs_error("Failed to open directory '%s': %s", dir_path,
                 strerror(errno));
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (g_stop) {
      closedir(dir);
      return;
    }

    /* Пропускаем . и .. */
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    /* Составляем полный путь */
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    /* Составляем display_name */
    char display_name[PATH_MAX];
#ifdef SHOW_FILE_RELATIVE_PATH
    if (prefix[0] == '\0') {
      snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
    } else {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix,
               entry->d_name);
    }
#else
    snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
#endif

    /* Используем lstat для информации о самом файле (не target) */
    struct stat st;
    if (lstat(full_path, &st) == -1) {
      log_fs_error("lstat failed for '%s': %s", full_path, strerror(errno));
      continue;
    }

    /* Определяем тип файла и размер */
    bool is_symlink = S_ISLNK(st.st_mode);
    bool is_dir = S_ISDIR(st.st_mode);
    off_t size_value = st.st_size;
    bool should_add = false;
    bool should_recurse = false;

    if (is_symlink) {
      /* Это симлинк */
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        /* Полностью игнорируем */
        continue;
      }

      /* Проверяем target для определения типа */
      struct stat target_st;
      bool target_exists = (stat(full_path, &target_st) == 0);

      if (!target_exists) {
        /* Broken symlink */
        log_fs_error("Broken symlink: '%s'", full_path);
        should_add = true;
      } else {
        /* Symlink указывает на существующий файл */
        should_add = true;
        if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_RECURSE &&
            S_ISDIR(target_st.st_mode) && depth < SYMLINK_RECURSE_MAX_DEPTH) {
          should_recurse = true;
        }
      }
    } else if (is_dir) {
      /* Это обычный каталог */
      should_add = true;
      should_recurse = true;
    } else {
      /* Это обычный файл */
      should_add = true;
    }

    if (should_add) {
      char size_str[32];
      char date_str[16];
      char perm_str[32];

      format_size(size_value, size_str, sizeof(size_str));
      format_date(st.st_mtime, date_str, sizeof(date_str));
      format_perms(st.st_mode, perm_str, sizeof(perm_str));

      add_file(display_name, size_str, date_str, perm_str);
    }

    if (should_recurse) {
      traverse_recursive(full_path, display_name, depth + 1);
    }
  }

  closedir(dir);
}

int traverse_fs(void *arg) {
  char *dir_path = (char *)arg;
  batch_count = 0;
  traverse_recursive(dir_path, "", 0);
  flush_batch();

  if (g_vscroll) {
    g_vscroll->total_virtual_rows = g_rows;
    g_vscroll->needs_reload = true;
  }

  free(dir_path);
  g_fs_traversing = false;
  return 0;
}