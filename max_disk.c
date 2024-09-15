#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>

#define MAX_PATH_LENGTH 1024
#define MAX_LINE_LENGTH 256

// 函数原型
unsigned long long get_free_disk_space(const char *path);
char* find_best_path(const char *filename, unsigned long long *max_free_space);
void print_path(const char *path, unsigned long long free_space, int short_output);

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s [-s] <path_list_file>\n", argv[0]);
        return 1;
    }

    int short_output = 0;
    const char *path_list_file;
    
    // 解析命令行参数
    if (argc == 3 && strcmp(argv[1], "-s") == 0) {
        short_output = 1;
        path_list_file = argv[2];
    } else if (argc == 2) {
        path_list_file = argv[1];
    } else {
        fprintf(stderr, "Usage: %s [-s] <path_list_file>\n", argv[0]);
        return 1;
    }

    unsigned long long max_free_space = 0;
    char *best_path = find_best_path(path_list_file, &max_free_space);

    if (best_path) {
        print_path(best_path, max_free_space, short_output);
        free(best_path);
    } else {
        printf("No valid paths found or error reading the file.\n");
    }

    return 0;
}

// 获取指定路径的可用磁盘空间
unsigned long long get_free_disk_space(const char *path) {
    struct statvfs stat;

    if (statvfs(path, &stat) != 0) {
        perror("statvfs");
        return 0;
    }

    return stat.f_bfree * stat.f_frsize;
}

// 从文件中选择一个可用容量最大的路径
char* find_best_path(const char *filename, unsigned long long *max_free_space) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen");
        return NULL;
    }

    char line[MAX_LINE_LENGTH];
    char *best_path = NULL;
    *max_free_space = 0;

    while (fgets(line, sizeof(line), file)) {
        // 去掉行末的换行符
        line[strcspn(line, "\n")] = '\0';

        // 计算路径的可用磁盘空间
        unsigned long long free_space = get_free_disk_space(line);

        if (free_space > *max_free_space) {
            *max_free_space = free_space;
            free(best_path);  // 释放之前的路径内存
            best_path = strdup(line);  // 复制当前路径
            if (best_path == NULL) {
                perror("strdup");
                fclose(file);
                return NULL;
            }
        }
    }

    fclose(file);
    return best_path;
}

// 打印路径
void print_path(const char *path, unsigned long long free_space, int short_output) {
    if (short_output) {
        printf("%s\n", path);
    } else {
        printf("Path with the largest available capacity: %s\n", path);
        printf("Available space: %llu bytes\n", free_space);
    }
}

