#include "include/fs.h"
#include "include/config.h"
#include "include/globals.h"
#include "include/utils.h"
#include "include/virtual_scroll.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* --- File batch info --- */
typedef struct {
  char *display_name;
  char *size_str;
  char *date_str;
  char *perm_str;
} FileInfo;

static FileInfo batch[BATCH_SIZE];
static int batch_count = 0;

/* Static storage for original and canonical path used in header substitution */
static char *fs_orig_path = NULL;
static char *fs_canon_path = NULL;

/* Helper: dynamic append to buffer */
static bool buf_append(char **bufp, size_t *cap, size_t *len, const char *src) {
  if (!src)
    return true;
  size_t need = strlen(src);
  if (*len + need + 1 > *cap) {
    size_t newcap = (*cap == 0) ? 128 : (*cap * 2);
    while (newcap < *len + need + 1)
      newcap *= 2;
    char *n = realloc(*bufp, newcap);
    if (!n)
      return false;
    *bufp = n;
    *cap = newcap;
  }
  memcpy((*bufp) + *len, src, need);
  *len += need;
  (*bufp)[*len] = '\0';
  return true;
}

/* Public function: Render a header template with substitutions.
 * Supported substitutions:
 *  %% -> %
 *  %P -> canonical path
 *  %p -> original path
 *  %b -> sum of all displayed sizes
 *  %f -> sum of regular files only
 *  %d -> actual disk usage
 *
 * Returns malloc'd string (caller must free). */
char *render_header_template(const char *tmpl) {
  if (!tmpl)
    return strdup("");
  char *out = NULL;
  size_t cap = 0, len = 0;

  for (size_t i = 0; tmpl[i] != '\0'; ++i) {
    if (tmpl[i] == '%' && tmpl[i + 1] != '\0') {
      char t = tmpl[++i];
      if (t == '%') {
        if (!buf_append(&out, &cap, &len, "%"))
          goto fail;
      } else if (t == 'P') {
        if (!buf_append(&out, &cap, &len, fs_canon_path ? fs_canon_path : ""))
          goto fail;
      } else if (t == 'p') {
        if (!buf_append(&out, &cap, &len, fs_orig_path ? fs_orig_path : ""))
          goto fail;
      } else if (t == 'b') {
        char numbuf[64];
        snprintf(numbuf, sizeof(numbuf), "%llu",
                 (unsigned long long)g_total_bytes);
        if (!buf_append(&out, &cap, &len, numbuf))
          goto fail;
      } else if (t == 'f') {
        char numbuf[64];
        snprintf(numbuf, sizeof(numbuf), "%llu",
                 (unsigned long long)g_total_file_bytes);
        if (!buf_append(&out, &cap, &len, numbuf))
          goto fail;
      } else if (t == 'd') {
        char numbuf[64];
        snprintf(numbuf, sizeof(numbuf), "%llu",
                 (unsigned long long)g_total_disk_bytes);
        if (!buf_append(&out, &cap, &len, numbuf))
          goto fail;
      } else {
        /* unknown escape: output '%' and the char */
        char tmp[3] = {'%', t, '\0'};
        if (!buf_append(&out, &cap, &len, tmp))
          goto fail;
      }
    } else {
      char tmp[2] = {tmpl[i], '\0'};
      if (!buf_append(&out, &cap, &len, tmp))
        goto fail;
    }
  }

  return out;

fail:
  free(out);
  return NULL;
}

/* --- flush/add batch as before --- */
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

    /* These set_cell calls now automatically update g_max_col_widths */
    set_cell(r, 0, batch[i].display_name);
    set_cell(r, 1, batch[i].size_str);
    set_cell(r, 2, batch[i].date_str);
    set_cell(r, 3, batch[i].perm_str);

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

/* add_file: now also receives is_regular_file to distinguish file vs directory
 * sizes */
static void add_file(const char *display_name, const char *size_str,
                     const char *date_str, const char *perm_str,
                     off_t size_value, bool is_regular_file, blkcnt_t blocks) {
  if (batch_count == BATCH_SIZE) {
    flush_batch();
  }

  batch[batch_count].display_name = strdup(display_name ? display_name : "");
  batch[batch_count].size_str = strdup(size_str ? size_str : "");
  batch[batch_count].date_str = strdup(date_str ? date_str : "");
  batch[batch_count].perm_str = strdup(perm_str ? perm_str : "");
  batch_count++;

  /* Update totals and header if needed (under mutex) */
  if (size_value > 0) {
    SDL_LockMutex(g_grid_mutex);
    g_total_bytes += (unsigned long long)size_value;
    if (is_regular_file) {
      g_total_file_bytes += (unsigned long long)size_value;
    }
    /* Disk usage: st.st_blocks is in 512-byte blocks */
    g_total_disk_bytes += (unsigned long long)blocks * 512;
    SDL_UnlockMutex(g_grid_mutex);
  }
}

static void format_size(off_t size, char *buf, size_t len) {
  snprintf(buf, len, "%lld", (long long)size);
}

/* format_date:
 * - Uses DATE_FORMAT_TEMPLATE from config.h. If it's empty, falls back to "%c".
 * - Uses localtime_r for thread-safety.
 * - Ensures buffer is NUL-terminated.
 */
static void format_date(time_t mtime, char *buf, size_t len) {
  if (buf == NULL || len == 0)
    return;

  struct tm tm_buf;
  struct tm *tmres = localtime_r(&mtime, &tm_buf);
  if (!tmres) {
    /* Failed to convert time; print numeric epoch as fallback */
    snprintf(buf, len, "%lld", (long long)mtime);
    return;
  }

  const char *fmt = DATE_FORMAT_TEMPLATE;
  if (!fmt || fmt[0] == '\0') {
    fmt = "%c";
  }

  size_t wrote = strftime(buf, len, fmt, &tm_buf);
  if (wrote == 0) {
    /* Fallback: localized default */
    if (strftime(buf, len, "%c", &tm_buf) == 0) {
      /* Last resort: numeric epoch */
      snprintf(buf, len, "%lld", (long long)mtime);
    }
  }
}

/* format_perms: unchanged except ensure buffer fits and is NUL-terminated */
static void format_perms(mode_t mode, char *buf, size_t len) {
  const char *template = PERM_TEMPLATE;
  int idx = 0;

  for (const char *p = template; *p && idx < (int)len - 1; p++) {
    if (*p == '%' && *(p + 1)) {
      p++; /* skip % */
      switch (*p) {
      case 'n': /* numeric permissions */
        idx += snprintf(buf + idx, len - idx, "%04o", (unsigned)(mode & 07777));
        break;

      case 'T': /* file type */
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
        break;

      case 'S': /* special bits separate */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : '-';
        break;

      case 'u': /* user rwx */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IRUSR) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWUSR) ? 'w' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IXUSR) ? 'x' : '-';
        break;

      case 'g': /* group rwx */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IRGRP) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWGRP) ? 'w' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IXGRP) ? 'x' : '-';
        break;

      case 'o': /* other rwx */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IROTH) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWOTH) ? 'w' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IXOTH) ? 'x' : '-';
        break;

      case 'U': /* user with embedded setuid */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IRUSR) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWUSR) ? 'w' : '-';
        if (mode & S_ISUID)
          buf[idx++] = (mode & S_IXUSR) ? 's' : 'S';
        else
          buf[idx++] = (mode & S_IXUSR) ? 'x' : '-';
        break;

      case 'G': /* group with embedded setgid */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IRGRP) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWGRP) ? 'w' : '-';
        if (mode & S_ISGID)
          buf[idx++] = (mode & S_IXGRP) ? 's' : 'S';
        else
          buf[idx++] = (mode & S_IXGRP) ? 'x' : '-';
        break;

      case 'O': /* other with embedded sticky */
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IROTH) ? 'r' : '-';
        if (idx < (int)len - 1)
          buf[idx++] = (mode & S_IWOTH) ? 'w' : '-';
        if (mode & S_ISVTX)
          buf[idx++] = (mode & S_IXOTH) ? 't' : 'T';
        else
          buf[idx++] = (mode & S_IXOTH) ? 'x' : '-';
        break;

      default:
        if (idx < (int)len - 1)
          buf[idx++] = '%';
        if (idx < (int)len - 1)
          buf[idx++] = *p;
        break;
      }
    } else {
      buf[idx++] = *p; /* literal character */
    }
  }

  buf[idx] = '\0';
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
    if (prefix && prefix[0] != '\0') {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix,
               entry->d_name);
    } else {
      snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
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
      char date_str[64]; /* увеличен размер для разнообразных шаблонов */
      char perm_str[64];

      format_size(size_value, size_str, sizeof(size_str));
      format_date(st.st_mtime, date_str, sizeof(date_str));
      format_perms(st.st_mode, perm_str, sizeof(perm_str));

      /* pass is_regular_file to distinguish file vs directory for size totals
       */
      bool is_regular_file = S_ISREG(st.st_mode);
      add_file(display_name, size_str, date_str, perm_str, size_value,
               is_regular_file, st.st_blocks);
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

  /* Save original path */
  if (fs_orig_path) {
    free(fs_orig_path);
    fs_orig_path = NULL;
  }
  fs_orig_path = strdup(dir_path ? dir_path : "");

  /* Compute canonical path once (may be NULL if fails) */
  if (fs_canon_path) {
    free(fs_canon_path);
    fs_canon_path = NULL;
  }
  fs_canon_path = realpath(dir_path, NULL);
  if (!fs_canon_path) {
    /* fallback to original */
    fs_canon_path = strdup(fs_orig_path ? fs_orig_path : "");
  }

  /* Reset total bytes for this traversal */
  SDL_LockMutex(g_grid_mutex);
  g_total_bytes = 0ULL;
  g_total_file_bytes = 0ULL;
  g_total_disk_bytes = 0ULL;
  /* Also, set initial headers once (before scanning) */
  SDL_UnlockMutex(g_grid_mutex);

  traverse_recursive(dir_path, "", 0);
  flush_batch();

  if (g_vscroll) {
    g_vscroll->total_virtual_rows = g_rows;
    g_vscroll->needs_reload = true;
  }

  free(dir_path);

  /* release canonical/orig strings */
  if (fs_orig_path) {
    free(fs_orig_path);
    fs_orig_path = NULL;
  }
  if (fs_canon_path) {
    free(fs_canon_path);
    fs_canon_path = NULL;
  }

  g_fs_traversing = false;
  return 0;
}