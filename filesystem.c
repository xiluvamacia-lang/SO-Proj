#define _XOPEN_SOURCE 700

#include "filesystem.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

int path_exists(const char *path){
  struct stat st;

  if (stat(path, &st) != 0)
    return 0;

  return S_ISDIR(st.st_mode);
}

int file_exists(const char *path){
  struct stat st;

  if (stat(path, &st) != 0)
    return 0;

  return S_ISREG(st.st_mode);
}

int absolute_path(const char *path, char *buffer, size_t size){
  char *resolved = realpath(path, NULL);

  if (resolved == NULL)
    return 1;

  if (strlen(resolved) >= size) {
    free(resolved);
    return 1;
  }
  strcpy(buffer, resolved);

  free(resolved);
  return 0;
}

int is_conf_file(const struct dirent *entry) {
  const char *name = entry->d_name;
  size_t len = strlen(name);
  
  return (len > 5 && strcmp(name + len - 5, ".conf") == 0);
}

static int copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return 1;

    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        close(in);
        return 1;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(out, buf + written, (size_t)(n - written));
            if (w <= 0) {
                close(in);
                close(out);
                return 1;
            }
            written += w;
        }
    }

    close(in);
    close(out);
    return (n < 0) ? 1 : 0;
}

int copy_directory_recursive(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return 1;

    if (S_ISDIR(st.st_mode)) {
        mkdir(dst, 0755);

        DIR *dir = opendir(src);
        if (!dir) return 1;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) continue;

            char s[1024], d[1024];
            snprintf(s, sizeof(s), "%s/%s", src, entry->d_name);
            snprintf(d, sizeof(d), "%s/%s", dst, entry->d_name);

            if (copy_directory_recursive(s, d) != 0) {
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
        return 0;
    }

    if (S_ISREG(st.st_mode)) {
        return copy_file(src, dst);
    }
    return 0;
}
