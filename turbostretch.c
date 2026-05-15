#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>


static uint8_t *read_file(const char *path, size_t *size_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    size_t cap = 1 << 16, size = 0;
    uint8_t *buffer = malloc(cap);
    if (!buffer) {
        close(fd);
        fprintf(stderr, "error: read buffer malloc\n");
        return NULL;
    }

    for (;;) {
        if (size == cap) {
            cap *= 2;
            uint8_t *temp = realloc(buffer, cap);
            if (!temp) {
                free(buffer);
                close(fd);
                fprintf(stderr, "error: realloc\n");
                return NULL;
            }
            buffer = temp;
        }
        ssize_t recv = read(fd, buffer + size, cap - size);
        if (recv < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "error: reading '%s': %s\n", path, strerror(errno));
            free(buffer);
            close(fd);
            return NULL;
        }
        if (recv == 0)
            break;
        size += (size_t) recv;
    }

    close(fd);

    if (size == 0) {
        fprintf(stderr, "error: '%s' is empty\n", path);
        free(buffer);
        return NULL;
    }

    *size_out = size;
    return buffer;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: ./turbostretch <filename.jpeg>\n");
        return EXIT_FAILURE;
    }

    /* ----- Read image to memory ----- */
    const char *in_path = argv[1];
    size_t jpeg_size = 0;
    uint8_t *jpeg_buffer = read_file(in_path, &jpeg_size);
    if (!jpeg_buffer)
        return EXIT_FAILURE;
    fprintf(stderr, "image read to memory\n");

    free(jpeg_buffer);
    fprintf(stderr, "heap alloc cleared\n");

    return EXIT_SUCCESS;
}
