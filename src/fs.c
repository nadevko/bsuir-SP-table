#include "main.h"
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
    /* Note: in error, we lose the batch, but continue */
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

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (lstat(full_path, &st) == -1) {
      log_fs_error("lstat failed for '%s': %s", full_path, strerror(errno));
      continue;
    }

    char display_name[PATH_MAX];
    if (prefix[0] == '\0') {
      snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
    } else {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix,
               entry->d_name);
    }

    bool is_sym = S_ISLNK(st.st_mode);
    bool is_dir = false;
    bool recurse = false;

    off_t size_value = 0;
    if (is_sym) {
      struct stat target_st;
      if (stat(full_path, &target_st) == 0) {
        size_value = target_st.st_size;
        is_dir = S_ISDIR(target_st.st_mode);
      }
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        continue;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_SKIP_CONTENT) {
        recurse = false;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_RECURSE) {
        recurse = is_dir && (depth < SYMLINK_RECURSE_MAX_DEPTH);
      }
    } else {
      size_value = st.st_size;
      is_dir = S_ISDIR(st.st_mode);
      recurse = is_dir;
    }

    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);

    char date_str[16];
    struct tm *mtime = localtime(&st.st_mtime);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);

    char perm_str[11] = "----------";
    mode_t mode = st.st_mode;
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
    perm_str[9] = '\0';

    add_file(display_name, size_str, date_str, perm_str);

    if (g_stop) {
      closedir(dir);
      return;
    }

    if (recurse) {
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
  free(dir_path);
  g_fs_traversing = false;
  return 0;
}