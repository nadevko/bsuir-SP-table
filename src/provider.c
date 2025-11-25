#include "include/provider.h"
#include "include/config.h"
#include "include/fileentry.h"
#include <SDL3/SDL.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* --- Filesystem Provider --- */

typedef struct {
  FileEntry **entries;
  int count;
  int capacity;
  char *root_path;
} FSProviderCtx;

static int fs_row_count(void *provider_ctx) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;
  return ctx ? ctx->count : 0;
}

static char *fs_get_cell(void *provider_ctx, int row, int col) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;

  if (!ctx || row < -1 || row >= ctx->count || col < 0 || col > 3)
    return strdup("");

  if (row == -1) {
    /* Header row */
    const char *headers[] = {HEADER_TEMPLATE_0, HEADER_TEMPLATE_1,
                             HEADER_TEMPLATE_2, HEADER_TEMPLATE_3};
    if (col < 4)
      return strdup(headers[col]);
    return strdup("");
  }

  FileEntry *entry = ctx->entries[row];
  if (!entry)
    return strdup("");

  switch (col) {
  case 0: /* Path */
    return strdup(entry->name ? entry->name : "");
  case 1: /* Size */
  {
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)entry->st.st_size);
    return strdup(buf);
  }
  case 2: /* Date */
  {
    char buf[128];
    struct tm tm_buf;
    const char *fmt = DATE_FORMAT_TEMPLATE;
    if (localtime_r(&entry->st.st_mtime, &tm_buf)) {
      strftime(buf, sizeof buf, fmt, &tm_buf);
    } else {
      strcpy(buf, "???");
    }
    return strdup(buf);
  }
  case 3: /* Permissions */
  {
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
  }

  return strdup("");
}

static void *fs_get_row_data(void *provider_ctx, int row) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;

  if (!ctx || row < 0 || row >= ctx->count)
    return NULL;

  return (void *)ctx->entries[row];
}

static bool fs_insert_row(void *provider_ctx, int row, void *data) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;

  if (!ctx || row < 0 || row > ctx->count)
    return false;

  if (ctx->count >= ctx->capacity) {
    int new_cap = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
    FileEntry **new_entries =
        realloc(ctx->entries, (size_t)new_cap * sizeof *new_entries);
    if (!new_entries)
      return false;
    ctx->entries = new_entries;
    ctx->capacity = new_cap;
  }

  /* Shift entries to the right */
  for (int i = ctx->count; i > row; i--) {
    ctx->entries[i] = ctx->entries[i - 1];
  }

  ctx->entries[row] = (FileEntry *)data;
  ctx->count++;

  return true;
}

static bool fs_delete_row(void *provider_ctx, int row) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;

  if (!ctx || row < 0 || row >= ctx->count)
    return false;

  /* Shift entries to the left */
  for (int i = row; i < ctx->count - 1; i++) {
    ctx->entries[i] = ctx->entries[i + 1];
  }

  ctx->count--;
  return true;
}

static void fs_destroy(void *provider_ctx) {
  FSProviderCtx *ctx = (FSProviderCtx *)provider_ctx;

  if (!ctx)
    return;

  for (int i = 0; i < ctx->count; i++) {
    if (ctx->entries[i]) {
      free(ctx->entries[i]->name);
      free(ctx->entries[i]->full_path);
      free(ctx->entries[i]->resolved_path);
      free(ctx->entries[i]->dir_path);
      free(ctx->entries[i]->root_path);
      free(ctx->entries[i]);
    }
  }

  free(ctx->entries);
  free(ctx->root_path);
  free(ctx);
}

DataProvider *provider_create_filesystem(const char *path) {
  if (!path)
    return NULL;

  DataProvider *provider = malloc(sizeof *provider);
  if (!provider)
    return NULL;

  FSProviderCtx *ctx = calloc(1, sizeof *ctx);
  if (!ctx) {
    free(provider);
    return NULL;
  }

  ctx->root_path = strdup(path);
  ctx->entries = malloc(16 * sizeof *ctx->entries);
  ctx->capacity = 16;
  ctx->count = 0;

  if (!ctx->entries || !ctx->root_path) {
    free(ctx->entries);
    free(ctx->root_path);
    free(ctx);
    free(provider);
    return NULL;
  }

  provider->ops.row_count = fs_row_count;
  provider->ops.get_cell = fs_get_cell;
  provider->ops.get_row_data = fs_get_row_data;
  provider->ops.insert_row = fs_insert_row;
  provider->ops.delete_row = fs_delete_row;
  provider->ops.destroy = fs_destroy;
  provider->ctx = ctx;

  return provider;
}

/* --- Dual-pane Provider --- */

typedef struct {
  DataProvider *left;
  DataProvider *right;
} DualProviderCtx;

static int dual_row_count(void *provider_ctx) {
  DualProviderCtx *ctx = (DualProviderCtx *)provider_ctx;

  if (!ctx || !ctx->left || !ctx->right)
    return 0;

  int left_count = ctx->left->ops.row_count(ctx->left->ctx);
  int right_count = ctx->right->ops.row_count(ctx->right->ctx);

  return SDL_max(left_count, right_count);
}

static char *dual_get_cell(void *provider_ctx, int row, int col) {
  DualProviderCtx *ctx = (DualProviderCtx *)provider_ctx;

  if (!ctx || !ctx->left || !ctx->right)
    return strdup("");

  /* Columns 0-1: left path + right path
   * Columns 2-3: left size + right size
   * Columns 4-5: left date + right date
   * etc. */

  int provider_col = col / 2;
  int side = col % 2; /* 0 = left, 1 = right */

  DataProvider *provider = side == 0 ? ctx->left : ctx->right;

  return provider->ops.get_cell(provider->ctx, row, provider_col);
}

static void *dual_get_row_data(void *provider_ctx, int row) {
  /* Return combined structure or NULL */
  (void)provider_ctx;
  (void)row;
  return NULL;
}

static bool dual_insert_row(void *provider_ctx, int row, void *data) {
  (void)provider_ctx;
  (void)row;
  (void)data;
  return false; /* Not supported for dual provider */
}

static bool dual_delete_row(void *provider_ctx, int row) {
  (void)provider_ctx;
  (void)row;
  return false; /* Not supported for dual provider */
}

static void dual_destroy(void *provider_ctx) {
  DualProviderCtx *ctx = (DualProviderCtx *)provider_ctx;

  if (!ctx)
    return;

  if (ctx->left)
    provider_destroy(ctx->left);
  if (ctx->right)
    provider_destroy(ctx->right);

  free(ctx);
}

DataProvider *provider_create_dual(DataProvider *left, DataProvider *right) {
  if (!left || !right)
    return NULL;

  DataProvider *provider = malloc(sizeof *provider);
  if (!provider)
    return NULL;

  DualProviderCtx *ctx = malloc(sizeof *ctx);
  if (!ctx) {
    free(provider);
    return NULL;
  }

  ctx->left = left;
  ctx->right = right;

  provider->ops.row_count = dual_row_count;
  provider->ops.get_cell = dual_get_cell;
  provider->ops.get_row_data = dual_get_row_data;
  provider->ops.insert_row = dual_insert_row;
  provider->ops.delete_row = dual_delete_row;
  provider->ops.destroy = dual_destroy;
  provider->ctx = ctx;

  return provider;
}

void provider_destroy(DataProvider *p) {
  if (!p)
    return;

  if (p->ops.destroy) {
    p->ops.destroy(p->ctx);
  }

  free(p);
}