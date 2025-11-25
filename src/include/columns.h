#pragma once

#include <stdbool.h>

typedef enum {
  COL_PATH,
  COL_SIZE,
  COL_DATE,
  COL_PERMS,
  COL_CUSTOM = 100
} ColumnType;

typedef struct {
  ColumnType type;
  const char *cell_template;
  const char *header_template;
  int width_min;
  int width_max;
  void *user_data;

  /* Custom renderers (if type == COL_CUSTOM) */
  char *(*render_cell)(void *user_data, void *row_data);
  char *(*render_header)(void *user_data);
} ColumnDef;

typedef struct {
  ColumnDef *columns;
  int count;
  int capacity;
} ColumnRegistry;

/* Create empty column registry */
ColumnRegistry *cols_create(void);

/* Add column to the end */
void cols_add(ColumnRegistry *reg, ColumnDef col);

/* Remove column at index */
void cols_remove(ColumnRegistry *reg, int col_idx);

/* Insert column at specific index */
void cols_insert(ColumnRegistry *reg, int col_idx, ColumnDef col);

/* Move column from src to dst index */
void cols_move(ColumnRegistry *reg, int src_idx, int dst_idx);

/* Get column definition */
ColumnDef *cols_get(ColumnRegistry *reg, int col_idx);

/* Destroy registry */
void cols_destroy(ColumnRegistry *reg);

/* Predefined column definitions */
ColumnDef col_path_default(void);
ColumnDef col_size_default(void);
ColumnDef col_date_default(void);
ColumnDef col_perms_default(void);