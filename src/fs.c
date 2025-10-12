#include "main.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Helpers to check symlink target */
static bool is_symlink_mode(mode_t mode) { return S_ISLNK(mode); }

/* ---------- Counting ---------- */

#ifdef RECURSIVE_LISTING
/* Recursive counting with symlink strategy and depth limit */
static int count_files_recursive_internal(const char *dir_path, int depth) {
  DIR *dir = opendir(dir_path);
  if (!dir) {
    log_fs_error("Failed to open directory '%s' for counting: %s", dir_path,
                 strerror(errno));
    return 0;
  }

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat st;
    if (lstat(full_path, &st) == -1) {
      log_fs_error("lstat failed for '%s': %s", full_path, strerror(errno));
      continue;
    }

    if (is_symlink_mode(st.st_mode)) {
      /* symlink handling */
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        continue;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_SKIP_CONTENT) {
        /* count the symlink entry, but do not recurse into it even if it points
         * to dir */
        count++;
        continue;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_RECURSE) {
        /* count entry. If it points to a directory and depth limit not reached,
         * follow */
        count++;
        if (depth >= SYMLINK_RECURSE_MAX_DEPTH) {
          /* depth limit reached */
          continue;
        }
        struct stat target_st;
        if (stat(full_path, &target_st) == -1) {
          /* cannot stat target: treat as non-directory */
          continue;
        }
        if (S_ISDIR(target_st.st_mode)) {
          count += count_files_recursive_internal(full_path, depth + 1);
        }
        continue;
      }
    }

    /* not a symlink */
    count++;
    if (S_ISDIR(st.st_mode)) {
      count += count_files_recursive_internal(full_path, depth + 1);
    }
  }
  closedir(dir);
  return count;
}
#endif

#ifndef RECURSIVE_LISTING
static int count_files_nonrecursive(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  if (!dir) {
    log_fs_error("Failed to open directory '%s' for counting: %s", dir_path,
                 strerror(errno));
    return 0;
  }

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    /* lstat to check symlink */
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
    struct stat st;
    if (lstat(full_path, &st) == -1) {
      log_fs_error("lstat failed for '%s': %s", full_path, strerror(errno));
      continue;
    }

    if (is_symlink_mode(st.st_mode)) {
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        continue;
      } else {
        /* LIST_SKIP_CONTENT or LIST_RECURSE -> include the symlink as entry */
        count++;
        continue;
      }
    }

    count++;
  }
  closedir(dir);
  return count;
}
#endif

int count_files(const char *dir_path) {
#ifdef RECURSIVE_LISTING
  return count_files_recursive_internal(dir_path, 0);
#else
  return count_files_nonrecursive(dir_path);
#endif
}

/* ---------- Population ---------- */

#ifdef RECURSIVE_LISTING
static void populate_files_recursive_internal(const char *dir_path,
                                              const char *prefix, int *row_ptr,
                                              int depth) {
  DIR *dir = opendir(dir_path);
  if (!dir) {
    log_fs_error("Failed to open directory '%s' for populate: %s", dir_path,
                 strerror(errno));
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    struct stat file_stat;
    if (lstat(full_path, &file_stat) == -1) {
      log_fs_error("lstat failed for '%s' during populate: %s", full_path,
                   strerror(errno));
      continue;
    }

    char display_name[PATH_MAX];
    if (prefix[0] == '\0') {
      snprintf(display_name, sizeof(display_name), "%s", entry->d_name);
    } else {
      snprintf(display_name, sizeof(display_name), "%s/%s", prefix,
               entry->d_name);
    }

    if (is_symlink_mode(file_stat.st_mode)) {
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        continue;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_SKIP_CONTENT) {
        /* output a line for the symlink (column 0 + size + date + perms) but do
         * not recurse */
        set_cell(*row_ptr, 0, display_name);
        char size_str[32];
        off_t size_value = 0;
        struct stat target_stat;
        if (stat(full_path, &target_stat) == -1) {
          size_value = 0;
        } else {
          size_value = target_stat.st_size;
        }
        snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
        set_cell(*row_ptr, 1, size_str);

        char date_str[16];
        struct tm *mtime = localtime(&file_stat.st_mtime);
        strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
        set_cell(*row_ptr, 2, date_str);

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
        continue;
      } else if (SYMLINK_BEHAVIOUR == SYMLINK_LIST_RECURSE) {
        /* output the symlink line and if target is directory, follow it */
        set_cell(*row_ptr, 0, display_name);
        char size_str[32];
        off_t size_value = 0;
        struct stat target_stat;
        if (stat(full_path, &target_stat) == -1) {
          size_value = 0;
        } else {
          size_value = target_stat.st_size;
        }
        snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
        set_cell(*row_ptr, 1, size_str);

        char date_str[16];
        struct tm *mtime = localtime(&file_stat.st_mtime);
        strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
        set_cell(*row_ptr, 2, date_str);

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

        if (depth < SYMLINK_RECURSE_MAX_DEPTH) {
          struct stat t;
          if (stat(full_path, &t) == 0 && S_ISDIR(t.st_mode)) {
            populate_files_recursive_internal(full_path, display_name, row_ptr,
                                              depth + 1);
          }
        } else {
          log_fs_error("Symlink recursion depth limit reached for '%s'",
                       full_path);
        }
        continue;
      }
    }

    /* not a symlink */
    set_cell(*row_ptr, 0, display_name);

    /* Size */
    char size_str[32];
    off_t size_value = 0;
    if (S_ISLNK(file_stat.st_mode)) {
      /* handled above */
      size_value = 0;
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
      populate_files_recursive_internal(full_path, display_name, row_ptr,
                                        depth + 1);
    }
  }
  closedir(dir);
}
#endif

#ifndef RECURSIVE_LISTING
static void populate_files_nonrecursive(const char *dir_path, int *row_ptr) {
  DIR *dir = opendir(dir_path);
  if (!dir) {
    log_fs_error("Failed to open directory '%s' for populate: %s", dir_path,
                 strerror(errno));
    return;
  }

  struct dirent *entry;
  char full_path[PATH_MAX];
  struct stat file_stat;
  while ((entry = readdir(dir)) && *row_ptr < g_rows) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

    if (lstat(full_path, &file_stat) == -1) {
      log_fs_error("Failed to lstat %s: %s", full_path, strerror(errno));
      continue;
    }

    if (is_symlink_mode(file_stat.st_mode)) {
      if (SYMLINK_BEHAVIOUR == SYMLINK_IGNORE) {
        continue;
      } else {
        /* LIST_SKIP_CONTENT or LIST_RECURSE -> include the symlink as entry */
        set_cell(*row_ptr, 0, entry->d_name);
        char size_str[32];
        off_t size_value = 0;
        struct stat target_stat;
        if (stat(full_path, &target_stat) == -1) {
          size_value = 0;
        } else {
          size_value = target_stat.st_size;
        }
        snprintf(size_str, sizeof(size_str), "%lld", (long long)size_value);
        set_cell(*row_ptr, 1, size_str);

        char date_str[16];
        struct tm *mtime = localtime(&file_stat.st_mtime);
        strftime(date_str, sizeof(date_str), "%d.%m.%Y", mtime);
        set_cell(*row_ptr, 2, date_str);

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

        /* For non-recursive mode, we do not descend into directories/symlinks.
         */
        continue;
      }
    }

    /* Column 0: Filename */
    set_cell(*row_ptr, 0, entry->d_name);

    /* Column 1: Size in bytes */
    char size_str[32];
    off_t size_value = 0;
    if (S_ISLNK(file_stat.st_mode)) {
      size_value = 0;
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
  populate_files_recursive_internal(dir_path, "", &r, 0);
#else
  int r = start_row;
  populate_files_nonrecursive(dir_path, &r);
#endif
}
