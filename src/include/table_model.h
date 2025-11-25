#pragma once

#include "columns.h"
#include "provider.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef struct {
  DataProvider *provider;
  ColumnRegistry *columns;

  /* Cached column widths */
  int *col_widths;

  /* Mutex for thread-safe access */
  SDL_Mutex *mutex;

  /* Dirty flags */
  bool widths_dirty;
  bool structure_dirty;
} TableModel;

/* Create table with provider and columns */
TableModel *table_create(DataProvider *provider, ColumnRegistry *cols);

/* Destroy table */
void table_destroy(TableModel *table);

/* Get rendered cell text (malloc'd, caller must free) */
char *table_get_cell(TableModel *table, int row, int col);

/* Get raw row data */
void *table_get_row_data(TableModel *table, int row);

/* Get total rows */
int table_get_row_count(TableModel *table);

/* Get total columns */
int table_get_col_count(TableModel *table);

/* Dynamic row operations */
bool table_insert_row(TableModel *table, int row, void *data);
bool table_delete_row(TableModel *table, int row);

/* Dynamic column operations */
bool table_add_column(TableModel *table, ColumnDef col);
bool table_insert_column(TableModel *table, int col_idx, ColumnDef col);
bool table_remove_column(TableModel *table, int col_idx);

/* Get column definition */
ColumnDef *table_get_column(TableModel *table, int col_idx);

/* Recalculate column widths based on font and padding */
void table_recalc_widths(TableModel *table, TTF_Font *font, int padding);

/* Get cached column width */
int table_get_col_width(TableModel *table, int col_idx);

/* Mark structure/widths as dirty for recalculation */
void table_mark_dirty(TableModel *table, bool widths, bool structure);

/* Is dirty */
bool table_is_widths_dirty(TableModel *table);
bool table_is_structure_dirty(TableModel *table);