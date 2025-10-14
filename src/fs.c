#include "include/fs.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/utils.h"
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

  // Normalize the input directory path
  char resolved_dir[PATH_MAX];
  if (!realpath(dir_path, resolved_dir)) {
    log_fs_error("Failed to resolve directory '%s': %s", dir_path,
                 strerror(errno));
    return;
  }

  DIR *dir = opendir(resolved_dir);
  if (!dir) {
    log_fs_error("Failed to open directory '%s': %s", resolved_dir,
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
    snprintf(full_path, sizeof(full_path), "%s/%s", resolved_dir,
             entry->d_name);

    // Normalize the full path
    char resolved_path[PATH_MAX];
    if (!realpath(full_path, resolved_path)) {
      log_fs_error("Failed to resolve path '%s': %s", full_path,
                   strerror(errno));
      continue;
    }

    struct stat st;
    if (lstat(resolved_path, &st) == -1) {
      log_fs_error("lstat failed for '%s': %s", resolved_path, strerror(errno));
      continue;
    }

    char display_name[PATH_MAX];
    // Use basename of resolved_path for display_name
    const char *basename = strrchr(resolved_path, '/');
    basename = basename ? basename + 1 : resolved_path;

#ifdef SHOW_FILE_RELATIVE_PATH
    if (prefix[0] == '\0') {
#endif
      snprintf(display_name, sizeof(display_name), "%s", basename);
#ifdef SHOW_FILE_RELATIVE_PATH
    } else {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix, basename);
    }
#endif

    bool is_sym = S_ISLNK(st.st_mode);
    bool is_dir = false;
    bool recurse = false;

    off_t size_value = 0;
    if (is_sym) {
      struct stat target_st;
      if (stat(resolved_path, &target_st) == 0) {
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

    /* ... (верх файла без изменений) ... */

    char perm_str[20];
#if PERM_FORMAT == PERM_NUMERIC
    snprintf(perm_str, sizeof(perm_str), "%04o",
             (unsigned)(st.st_mode & 07777));
#else // PERM_SYMBOLIC
    /* Build permission string with support for:
     * - SHOW_FILE_TYPE
     * - SHORTENED_EXTRA_BITS (default on)
     * - ADD_PERMISSIONS_WHITESPACES
     *
     * Two modes:
     * 1) SHORTENED_EXTRA_BITS == 1 (default) : ls-like merging of special bits
     * into execute positions example: -rwsr-sr-t 2) SHORTENED_EXTRA_BITS == 0 :
     * print three extra bits (SUID, SGID, STICKY) BEFORE file-type example: s g
     * t -rwx rw- r--
     *
     * If ADD_PERMISSIONS_WHITESPACES == 1, add spaces between user/group/other
     * groups.
     */
    int idx = 0;

    /* prepare user/group/other triplets (without merged special bits) */
    char user[4], group[4], other[4];
    mode_t mode = st.st_mode;

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

    /* compute extra bits representation (SUID, SGID, STICKY) */
    char extra[4] = {'-', '-', '-', '\0'};
    if (mode & S_ISUID)
      extra[0] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
      extra[1] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
      extra[2] = (mode & S_IXOTH) ? 't' : 'T';

#if SHORTENED_EXTRA_BITS
    /* merge special bits into execute positions (ls-like) */
    if (mode & S_ISUID)
      user[2] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
      group[2] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
      other[2] = (mode & S_IXOTH) ? 't' : 'T';
#endif

#if !SHORTENED_EXTRA_BITS
    perm_str[idx++] = extra[0];
    perm_str[idx++] = extra[1];
    perm_str[idx++] = extra[2];

#if ADD_PERMISSIONS_WHITESPACES
    perm_str[idx++] = ' ';
#endif

#endif

    if (SHOW_FILE_TYPE) {
      if (S_ISDIR(st.st_mode))
        perm_str[idx++] = 'd';
      else if (S_ISLNK(st.st_mode))
        perm_str[idx++] = 'l';
      else if (S_ISREG(st.st_mode))
        perm_str[idx++] = '-';
      else if (S_ISCHR(st.st_mode))
        perm_str[idx++] = 'c';
      else if (S_ISBLK(st.st_mode))
        perm_str[idx++] = 'b';
      else if (S_ISFIFO(st.st_mode))
        perm_str[idx++] = 'p';
      else if (S_ISSOCK(st.st_mode))
        perm_str[idx++] = 's';
      else
        perm_str[idx++] = '?';
    }

    for (int i = 0; i < 3; ++i)
      perm_str[idx++] = user[i];

#if ADD_PERMISSIONS_WHITESPACES
    perm_str[idx++] = ' ';
#endif

    for (int i = 0; i < 3; ++i)
      perm_str[idx++] = group[i];

#if ADD_PERMISSIONS_WHITESPACES
    perm_str[idx++] = ' ';
#endif

    for (int i = 0; i < 3; ++i)
      perm_str[idx++] = other[i];

    perm_str[idx] = '\0';
#endif

    add_file(display_name, size_str, date_str, perm_str);

    if (g_stop) {
      closedir(dir);
      return;
    }

    if (recurse) {
      traverse_recursive(resolved_path, display_name, depth + 1);
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
