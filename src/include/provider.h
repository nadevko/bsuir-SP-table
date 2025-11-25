#pragma once

#include "fileentry.h"
#include <stdbool.h>

/* Data provider interface for different data sources */
typedef struct {
  /* Get total number of rows */
  int (*row_count)(void *provider_ctx);

  /* Get rendered cell text for given row and column.
   * Returns malloc'd string (caller must free) */
  char *(*get_cell)(void *provider_ctx, int row, int col);

  /* Get raw row data (FileEntry pointer for FS, etc).
   * May return NULL for merged/virtual rows */
  void *(*get_row_data)(void *provider_ctx, int row);

  /* Add row at position. Data ownership depends on provider */
  bool (*insert_row)(void *provider_ctx, int row, void *data);

  /* Remove row at position */
  bool (*delete_row)(void *provider_ctx, int row);

  /* Cleanup provider context */
  void (*destroy)(void *provider_ctx);
} ProviderOps;

typedef struct {
  ProviderOps ops;
  void *ctx; /* Private provider context */
} DataProvider;

/* Create filesystem provider that traverses directory */
DataProvider *provider_create_filesystem(const char *path);

/* Create dual-pane provider combining two directories side-by-side */
DataProvider *provider_create_dual(DataProvider *left, DataProvider *right);

/* Destroy provider */
void provider_destroy(DataProvider *p);