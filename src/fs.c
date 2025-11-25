#include "include/fs.h"
#include "include/config.h"
#include "include/fileentry.h"
#include "include/globals.h"
#include "include/table_model.h"
#include <SDL3/SDL.h>
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
  FileEntry *entry;
} FileBatch;

static FileBatch batch[BATCH_SIZE];
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

/* Public function: Render a header template with substitutions. */
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
        snprintf(numbuf, sizeof numbuf, "%llu",
                 (unsigned long long)g_total_bytes);
        if (!buf_append(&out, &cap, &len, numbuf))
          goto fail;
      } else if (t == 'f') {
        char numbuf[64];
        snprintf(numbuf, sizeof numbuf, "%llu",
                 (unsigned long long)g_total_file_bytes);
        if (!buf_append(&out, &cap, &len, numbuf))
          goto fail;
      } else if (t == 'd') {
        char numbuf[64];
        snprintf(numbuf, sizeof numbuf, "%llu",
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

  /* Get filesystem provider context and insert rows */
  for (int i = 0; i < batch_count; i++) {
    if (!table_insert_row(g_table, table_get_row_count(g_table),
                          batch[i].entry)) {
      fprintf(stderr, "Failed to insert row into table\n");
    }
  }

  if (g_vscroll) {
    g_vscroll->total_virtual_rows = table_get_row_count(g_table) + 1;
    g_vscroll->needs_reload = true;
  }

  SDL_UnlockMutex(g_grid_mutex);

  batch_count = 0;
}

/* add_file: creates FileEntry and adds to batch */
static void add_file(const char *display_name, const char *full_path,
                     const char *dir_path, const char *root_path,
                     struct stat *st, bool is_broken_symlink) {
  if (batch_count == BATCH_SIZE) {
    flush_batch();
  }

  FileEntry *entry = calloc(1, sizeof *entry);
  if (!entry)
    return;

  entry->name = strdup(display_name ? display_name : "");
  entry->full_path = strdup(full_path ? full_path : "");
  entry->dir_path = strdup(dir_path ? dir_path : "");
  entry->root_path = strdup(root_path ? root_path : "");

  if (st) {
    entry->st = *st;
  }

  entry->is_regular_file = S_ISREG(st->st_mode);
  entry->is_broken_symlink = is_broken_symlink;

  char resolved[PATH_MAX];
  if (realpath(full_path, resolved)) {
    entry->resolved_path = strdup(resolved);
  }

  batch[batch_count].entry = entry;
  batch_count++;

  /* Update totals */
  if (st->st_size > 0) {
    SDL_LockMutex(g_grid_mutex);
    g_total_bytes += (unsigned long long)st->st_size;
    if (S_ISREG(st->st_mode)) {
      g_total_file_bytes += (unsigned long long)st->st_size;
    }
    g_total_disk_bytes += (unsigned long long)st->st_blocks * 512;
    SDL_UnlockMutex(g_grid_mutex);
  }
}

static void traverse_recursive(const char *dir_path, const char *prefix,
                               int depth) {
  if (g_stop)
    return;

  DIR *dir = opendir(dir_path);
  if (!dir) {
    fprintf(stderr, "Failed to open directory '%s': %s\n", dir_path,
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
    snprintf(full_path, sizeof full_path, "%s/%s", dir_path, entry->d_name);

    /* Составляем display_name */
    char display_name[PATH_MAX];
#ifdef SHOW_FILE_RELATIVE_PATH
    if (prefix && prefix[0] != '\0') {
      snprintf(display_name, sizeof display_name, "%s/%s", prefix,
               entry->d_name);
    } else {
      snprintf(display_name, sizeof display_name, "%s", entry->d_name);
    }
#else
    snprintf(display_name, sizeof display_name, "%s", entry->d_name);
#endif

    /* Используем lstat для информации о самом файле (не target) */
    struct stat st;
    if (lstat(full_path, &st) == -1) {
      fprintf(stderr, "lstat failed for '%s': %s\n", full_path,
              strerror(errno));
      continue;
    }

    /* Определяем тип файла и размер */
    bool is_symlink = S_ISLNK(st.st_mode);
    bool is_dir = S_ISDIR(st.st_mode);
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
        fprintf(stderr, "Broken symlink: '%s'\n", full_path);
        should_add = true;
        add_file(display_name, full_path, dir_path,
                 fs_orig_path ? fs_orig_path : dir_path, &st, true);
      } else {
        /* Symlink указывает на существующий файл */
        should_add = true;
        add_file(display_name, full_path, dir_path,
                 fs_orig_path ? fs_orig_path : dir_path, &target_st, false);

        if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_RECURSE &&
            S_ISDIR(target_st.st_mode) && depth < SYMLINK_RECURSE_MAX_DEPTH) {
          should_recurse = true;
        }
      }
    } else if (is_dir) {
      /* Это обычный каталог */
      should_add = true;
      add_file(display_name, full_path, dir_path,
               fs_orig_path ? fs_orig_path : dir_path, &st, false);
      should_recurse = true;
    } else {
      /* Это обычный файл */
      should_add = true;
      add_file(display_name, full_path, dir_path,
               fs_orig_path ? fs_orig_path : dir_path, &st, false);
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
  SDL_UnlockMutex(g_grid_mutex);

  traverse_recursive(dir_path, "", 0);
  flush_batch();

  if (g_vscroll) {
    g_vscroll->total_virtual_rows = table_get_row_count(g_table) + 1;
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