#include "main.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Recursive count */
#ifdef RECURSIVE_LISTING
static int count_files_recursive(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  if (!dir)
    return 0;

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (lstat(full_path, &st) == -1)
      continue;

    count++;
    if (S_ISDIR(st.st_mode)) {
      count += count_files_recursive(full_path);
    }
  }
  closedir(dir);
  return count;
}
#else
static int count_files_nonrecursive(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  if (!dir)
    return 0;

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    count++;
  }
  closedir(dir);
  return count;
}
#endif

int count_files(const char *dir_path) {
#ifdef RECURSIVE_LISTING
  return count_files_recursive(dir_path);
#else
  return count_files_nonrecursive(dir_path);
#endif
}

#ifdef RECURSIVE_LISTING
/* Recursive data filling */
static void populate_files_recursive(const char *dir_path, const char *prefix,
                                     int *row_ptr) {
  DIR *dir = opendir(dir_path);
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat file_stat;
    if (lstat(full_path, &file_stat) == -1)
      continue;

    char display_name[PATH_MAX];
    if (prefix[0] == '\0') {
      snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
    } else {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix,
               entry->d_name);
    }

    set_cell(*row_ptr, 0, display_name);

    /* Size */
    char size_str[32];
    off_t size_value = 0;
    if (S_ISLNK(file_stat.st_mode)) {
      struct stat target_stat;
      if (stat(full_path, &target_stat) == -1) {
        size_value = 0;
      } else {
        size_value = target_stat.st_size;
      }
    } else {
      size_value = file_stat.st_size;
    }
    snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
    set_cell(*row_ptr, 1, size_str);

    /* Date */
    char date_str[16];
    struct tm *mtime = localtime(&file_stat.st_mtime);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
    set_cell(*row_ptr, 2, date_str);

    /* Permissions */
    char perm_str[11] = "----------";
    mode_t mode = file_stat.st_mode;
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
    set_cell(*row_ptr, 3, perm_str);

    (*row_ptr)++;

    if (S_ISDIR(file_stat.st_mode)) {
      populate_files_recursive(full_path, display_name, row_ptr);
    }
  }
  closedir(dir);
}
#else
static void populate_files_nonrecursive(const char *dir_path, int *row_ptr) {
  DIR *dir = opendir(dir_path);
  if (!dir)
    return;

  struct dirent *entry;
  char full_path[PATH_MAX];
  struct stat file_stat;
  while ((entry = readdir(dir)) && *row_ptr < g_rows) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    if (lstat(full_path, &file_stat) == -1) {
      fprintf(stderr, "Failed to lstat %s: %s\n", full_path, strerror(errno));
      continue;
    }

    /* Column 0: Filename */
    set_cell(*row_ptr, 0, entry->d_name);

    /* Column 1: Size in bytes */
    char size_str[32];
    off_t size_value = 0;
    if (S_ISLNK(file_stat.st_mode)) {
      struct stat target_stat;
      if (stat(full_path, &target_stat) == -1) {
        size_value = 0;
      } else {
        size_value = target_stat.st_size;
      }
    } else {
      size_value = file_stat.st_size;
    }
    snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
    set_cell(*row_ptr, 1, size_str);

    /* Column 2: Date */
    char date_str[16];
    struct tm *mtime = localtime(&file_stat.st_mtime);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
    set_cell(*row_ptr, 2, date_str);

    /* Column 3: Permissions */
    char perm_str[11] = "----------";
    mode_t mode = file_stat.st_mode;
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
    set_cell(*row_ptr, 3, perm_str);

    (*row_ptr)++;
  }
  closedir(dir);
}
#endif

void populate_files(const char *dir_path, int start_row) {
#ifdef RECURSIVE_LISTING
  int r = start_row;
  populate_files_recursive(dir_path, "", &r);
#else
  int r = start_row;
  populate_files_nonrecursive(dir_path, &r);
#endif
}
