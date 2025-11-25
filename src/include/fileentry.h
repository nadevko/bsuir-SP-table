#pragma once

#include <stdbool.h>
#include <sys/stat.h>

typedef struct {
  char *name;
  char *full_path;
  char *resolved_path;
  char *dir_path;
  char *root_path;

  struct stat st;
  bool is_regular_file;
  bool is_broken_symlink;
} FileEntry;