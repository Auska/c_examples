#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "bencode.h"

static void print_element(const struct bencode *ctx) {
    int type = bencode_next(ctx);
    switch (type) {
        case BENCODE_INTEGER:
            printf("Integer: %.*s\n", (int)ctx->toklen, (char *)ctx->tok);
            break;
        case BENCODE_STRING:
            printf("%.*s\n", (int)ctx->toklen, (char *)ctx->tok);
            break;
        case BENCODE_LIST_BEGIN:
            printf("List Begin\n");
            break;
        case BENCODE_LIST_END:
            printf("List End\n");
            break;
        case BENCODE_DICT_BEGIN:
            printf("Dictionary Begin\n");
            break;
        case BENCODE_DICT_END:
            printf("Dictionary End\n");
            break;
        default:
            printf("Error: %d\n", type);
            break;
    }
}

static void parse_torrent(const unsigned char *data, size_t length) {
    struct bencode ctx;
    bencode_init(&ctx, data, length);

    int result;
    while ((result = bencode_next(&ctx)) != BENCODE_DONE) {
        if (result < 0) {
            fprintf(stderr, "Error: %d\n", result);
            bencode_free(&ctx);
            return;
        }

    if (result == BENCODE_STRING){
        if (ctx.toklen == 4 && memcmp(ctx.tok, "name", 4) == 0) {
            print_element(&ctx);
        }
    }
//        if (result == BENCODE_DICT_BEGIN) {
//            printf("Dictionary Begin\n");
//        } else if (result == BENCODE_DICT_END) {
//            printf("Dictionary End\n");
//        } else if (result == BENCODE_STRING) {
//            // Print dictionary keys
//            printf("Key: %.*s\n", (int)ctx.toklen, (char *)ctx.tok);
//            // Look for 'info' key to start special processing
//            if (ctx.toklen == 4 && memcmp(ctx.tok, "info", 4) == 0) {
//                printf("Found 'info' key\n");
//                // Process 'info' dictionary if needed
//            }
//        } else {
//            print_element(&ctx);
//        }
    }

    bencode_free(&ctx);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <torrent_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        perror("ftell");
        fclose(file);
        return EXIT_FAILURE;
    }

    unsigned char *data = malloc(file_size);
    if (!data) {
        perror("malloc");
        fclose(file);
        return EXIT_FAILURE;
    }

    if (fread(data, 1, file_size, file) != (size_t)file_size) {
        perror("fread");
        free(data);
        fclose(file);
        return EXIT_FAILURE;
    }

    fclose(file);

    parse_torrent(data, (size_t)file_size);

    free(data);
    return EXIT_SUCCESS;
}

