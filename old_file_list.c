#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#define MAX_PATH 1024

// Node structure for a linked list
typedef struct FileNode {
    char path[MAX_PATH];
    time_t mod_time;
    struct FileNode* next;
} FileNode;

// Function to create a new FileNode
FileNode* create_node(const char* path, time_t mod_time) {
    FileNode* new_node = (FileNode*)malloc(sizeof(FileNode));
    if (new_node == NULL) {
        perror("Unable to allocate memory");
        exit(EXIT_FAILURE);
    }
    strncpy(new_node->path, path, MAX_PATH);
    new_node->mod_time = mod_time;
    new_node->next = NULL;
    return new_node;
}

// Function to insert a node into the linked list in sorted order
void insert_sorted(FileNode** head, FileNode* new_node) {
    FileNode** current = head;
    while (*current && (*current)->mod_time < new_node->mod_time) {
        current = &(*current)->next;
    }
    new_node->next = *current;
    *current = new_node;
}

// Function to recursively traverse directories and collect file info
void traverse_directory(const char* dir_path, FileNode** file_list) {
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip "." and ".."
        }

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) == -1) {
            perror("Unable to get file status");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Recursively traverse subdirectory
            traverse_directory(path, file_list);
        } else if (S_ISREG(statbuf.st_mode)) {
            // Insert file info into the list
            FileNode* new_node = create_node(path, statbuf.st_mtime);
            insert_sorted(file_list, new_node);
        }
    }

    closedir(dir);
}

// Function to print the file list
void print_file_list(const FileNode* head) {
    while (head) {
        char time_buf[64];
        struct tm* tm_info = localtime(&head->mod_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("%s %s\n", time_buf, head->path);
        head = head->next;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* directory = argv[1];
    FileNode* file_list = NULL;

    traverse_directory(directory, &file_list);
    print_file_list(file_list);

    // Free the list
    FileNode* current = file_list;
    while (current) {
        FileNode* next = current->next;
        free(current);
        current = next;
    }

    return EXIT_SUCCESS;
}
