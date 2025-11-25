#include "include/table_model.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>

TableModel *table_create(DataProvider *provider, ColumnRegistry *cols) {
  if (!provider || !cols)
    return NULL;

  TableModel *table = malloc(sizeof *table);
  if (!table)
    return NULL;

  table->provider = provider;
  table->columns = cols;
  table->col_widths = NULL;
  table->mutex = SDL_CreateMutex();
  table->widths_dirty = true;
  table->structure_dirty = false;

  if (!table->mutex) {
    free(table);
    return NULL;
  }

  /* Allocate initial column widths */
  if (cols->count > 0) {
    table->col_widths = calloc((size_t)cols->count, sizeof *table->col_widths);
    if (!table->col_widths) {
      SDL_DestroyMutex(table->mutex);
      free(table);
      return NULL;
    }

    /* Initialize with minimum widths */
    for (int c = 0; c < cols->count; c++) {
      table->col_widths[c] = cols->columns[c].width_min;
    }
  }

  return table;
}

void table_destroy(TableModel *table) {
  if (!table)
    return;

  if (table->provider) {
    provider_destroy(table->provider);
  }

  if (table->columns) {
    cols_destroy(table->columns);
  }

  free(table->col_widths);

  if (table->mutex) {
    SDL_DestroyMutex(table->mutex);
  }

  free(table);
}

char *table_get_cell(TableModel *table, int row, int col) {
  if (!table || row < 0 || col < 0)
    return strdup("");

  SDL_LockMutex(table->mutex);

  if (col >= table->columns->count) {
    SDL_UnlockMutex(table->mutex);
    return strdup("");
  }

  int row_count = table->provider->ops.row_count(table->provider->ctx);
  if (row >= row_count) {
    SDL_UnlockMutex(table->mutex);
    return strdup("");
  }

  ColumnDef *col_def = &table->columns->columns[col];
  void *row_data = table->provider->ops.get_row_data(table->provider->ctx, row);

  char *result = NULL;

  /* Use custom renderer if available */
  if (col_def->render_cell && row_data) {
    result = col_def->render_cell((void *)col_def, row_data);
  } else {
    /* Fallback: get from provider */
    result = table->provider->ops.get_cell(table->provider->ctx, row, col);
  }

  SDL_UnlockMutex(table->mutex);

  return result ? result : strdup("");
}

void *table_get_row_data(TableModel *table, int row) {
  if (!table || row < 0)
    return NULL;

  SDL_LockMutex(table->mutex);

  int row_count = table->provider->ops.row_count(table->provider->ctx);
  if (row >= row_count) {
    SDL_UnlockMutex(table->mutex);
    return NULL;
  }

  void *result = table->provider->ops.get_row_data(table->provider->ctx, row);

  SDL_UnlockMutex(table->mutex);

  return result;
}

int table_get_row_count(TableModel *table) {
  if (!table)
    return 0;

  SDL_LockMutex(table->mutex);
  int count = table->provider->ops.row_count(table->provider->ctx);
  SDL_UnlockMutex(table->mutex);

  return count;
}

int table_get_col_count(TableModel *table) {
  if (!table)
    return 0;

  SDL_LockMutex(table->mutex);
  int count = table->columns->count;
  SDL_UnlockMutex(table->mutex);

  return count;
}

bool table_insert_row(TableModel *table, int row, void *data) {
  if (!table)
    return false;

  SDL_LockMutex(table->mutex);
  bool result =
      table->provider->ops.insert_row(table->provider->ctx, row, data);
  SDL_UnlockMutex(table->mutex);

  return result;
}

bool table_delete_row(TableModel *table, int row) {
  if (!table)
    return false;

  SDL_LockMutex(table->mutex);
  bool result = table->provider->ops.delete_row(table->provider->ctx, row);
  SDL_UnlockMutex(table->mutex);

  return result;
}

bool table_add_column(TableModel *table, ColumnDef col) {
  if (!table)
    return false;

  SDL_LockMutex(table->mutex);

  cols_add(table->columns, col);

  /* Reallocate widths array */
  int *new_widths = realloc(table->col_widths,
                            (size_t)table->columns->count * sizeof *new_widths);
  if (!new_widths) {
    cols_remove(table->columns, table->columns->count - 1);
    SDL_UnlockMutex(table->mutex);
    return false;
  }

  table->col_widths = new_widths;
  table->col_widths[table->columns->count - 1] = col.width_min;
  table->widths_dirty = true;
  table->structure_dirty = true;

  SDL_UnlockMutex(table->mutex);

  return true;
}

/* Helper: Get header text for a column */
char *table_get_header(TableModel *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= table->columns->count)
    return strdup("");

  ColumnDef *col_def = &table->columns->columns[col_idx];

  /* Use custom renderer if available */
  if (col_def->render_header) {
    return col_def->render_header(col_def->user_data);
  }

  /* Fallback to header_template */
  if (col_def->header_template) {
    return strdup(col_def->header_template);
  }

  return strdup("");
}

bool table_insert_column(TableModel *table, int col_idx, ColumnDef col) {
  if (!table || col_idx < 0 || col_idx > table->columns->count)
    return false;

  SDL_LockMutex(table->mutex);

  cols_insert(table->columns, col_idx, col);

  /* Reallocate widths array */
  int *new_widths = realloc(table->col_widths,
                            (size_t)table->columns->count * sizeof *new_widths);
  if (!new_widths) {
    cols_remove(table->columns, col_idx);
    SDL_UnlockMutex(table->mutex);
    return false;
  }

  table->col_widths = new_widths;

  /* Shift widths */
  for (int i = table->columns->count - 1; i > col_idx; i--) {
    table->col_widths[i] = table->col_widths[i - 1];
  }
  table->col_widths[col_idx] = col.width_min;

  table->widths_dirty = true;
  table->structure_dirty = true;

  SDL_UnlockMutex(table->mutex);

  return true;
}

bool table_remove_column(TableModel *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= table->columns->count)
    return false;

  SDL_LockMutex(table->mutex);

  if (table->columns->count <= 1) {
    SDL_UnlockMutex(table->mutex);
    return false; /* Keep at least one column */
  }

  cols_remove(table->columns, col_idx);

  /* Reallocate widths array */
  int *new_widths = realloc(table->col_widths,
                            (size_t)table->columns->count * sizeof *new_widths);
  if (!new_widths) {
    /* Restore column on allocation failure */
    cols_insert(table->columns, col_idx, (ColumnDef){0});
    SDL_UnlockMutex(table->mutex);
    return false;
  }

  table->col_widths = new_widths;
  table->widths_dirty = true;
  table->structure_dirty = true;

  SDL_UnlockMutex(table->mutex);

  return true;
}

ColumnDef *table_get_column(TableModel *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= table->columns->count)
    return NULL;

  SDL_LockMutex(table->mutex);
  ColumnDef *result = &table->columns->columns[col_idx];
  SDL_UnlockMutex(table->mutex);

  return result;
}

void table_recalc_widths(TableModel *table, TTF_Font *font, int padding) {
  if (!table || !font || !table->col_widths)
    return;

  SDL_LockMutex(table->mutex);

  int row_count = table->provider->ops.row_count(table->provider->ctx);

  /* Step 1: Calculate content-based widths (at least width_min) */
  for (int c = 0; c < table->columns->count; c++) {
    int max_width = table->columns->columns[c].width_min;

    /* Check header */
    char *header = table->provider->ops.get_cell(table->provider->ctx, -1, c);
    if (header) {
      int w, h;
      size_t len = strlen(header);
      if (TTF_GetStringSize(font, header, len, &w, &h)) {
        max_width = SDL_max(max_width, w + 2 * padding);
      }
      free(header);
    }

    /* Sample first N rows for width */
    int sample_size = SDL_min(100, row_count);
    for (int r = 0; r < sample_size; r++) {
      char *cell = table->provider->ops.get_cell(table->provider->ctx, r, c);
      if (cell) {
        int w, h;
        size_t len = strlen(cell);
        if (TTF_GetStringSize(font, cell, len, &w, &h)) {
          max_width = SDL_max(max_width, w + 2 * padding);
        }
        free(cell);
      }
    }

    /* Clamp to max */
    if (table->columns->columns[c].width_max > 0) {
      max_width = SDL_min(max_width, table->columns->columns[c].width_max);
    }

    table->col_widths[c] = max_width;
  }

  table->widths_dirty = false;

  SDL_UnlockMutex(table->mutex);
}

int table_get_col_width(TableModel *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= table->columns->count)
    return 0;

  SDL_LockMutex(table->mutex);
  int width = table->col_widths[col_idx];
  SDL_UnlockMutex(table->mutex);

  return width;
}

void table_mark_dirty(TableModel *table, bool widths, bool structure) {
  if (!table)
    return;

  SDL_LockMutex(table->mutex);
  if (widths)
    table->widths_dirty = true;
  if (structure)
    table->structure_dirty = true;
  SDL_UnlockMutex(table->mutex);
}

bool table_is_widths_dirty(TableModel *table) {
  if (!table)
    return false;

  SDL_LockMutex(table->mutex);
  bool dirty = table->widths_dirty;
  SDL_UnlockMutex(table->mutex);

  return dirty;
}

bool table_is_structure_dirty(TableModel *table) {
  if (!table)
    return false;

  SDL_LockMutex(table->mutex);
  bool dirty = table->structure_dirty;
  SDL_UnlockMutex(table->mutex);

  return dirty;
}