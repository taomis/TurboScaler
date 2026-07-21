#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

#include <turbojpeg.h>

#define JPEG_QUALITY 90


typedef struct {
    size_t jpeg_size;
    uint8_t *jpeg_buffer;
    uint8_t *rgb;
    int h, w;
} img_t;


static void clean(img_t **frame_ptr, tjhandle *handler, uint8_t *out_jpeg) {
    if (handler && *handler) {
        tj3Destroy(*handler);
        *handler = NULL;
    }

    if (frame_ptr && *frame_ptr) {
        if ((*frame_ptr)->rgb)
            tj3Free((*frame_ptr)->rgb);
        if ((*frame_ptr)->jpeg_buffer)
            free((*frame_ptr)->jpeg_buffer);
        free(*frame_ptr);
        *frame_ptr = NULL;
    }

    if (out_jpeg)
        tj3Free(out_jpeg);
}


static const char *err_str(tjhandle h) {
    char *s = tj3GetErrorStr(h);
    return s ? s : "unknown error";
}


static int build_output_path(const char *in_path, char *out, size_t out_size) {
    const char *slash = strrchr(in_path, '/');
    size_t dir_len = slash ? (size_t) (slash - in_path) + 1 : 0;
    const char *dot = strrchr(in_path + dir_len, '.');
    size_t stem_len = dot ? (size_t) (dot - in_path) - dir_len
                          : strlen(in_path) - dir_len;
    int n = snprintf(out, out_size, "%.*s%.*s_copy%s",
                     (int) dir_len, in_path,
                     (int) stem_len, in_path + dir_len,
                     dot ? dot : ".jpeg");
    return (n > 0 && (size_t) n < out_size) ? 0 : -1;
}


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


static int write_file(const char *out_path, uint8_t *out_jpeg, size_t out_jpeg_size) {
    int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "error: cannot open '%s' for writing: %s\n", out_path, strerror(errno));
        return -1;
    }

    size_t total_written = 0;
    while (total_written < out_jpeg_size) {
        ssize_t res = write(fd, out_jpeg + total_written, out_jpeg_size - total_written);
        if (res < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "error: writing '%s': %s\n", out_path, strerror(errno));
            close(fd);
            return -1;
        }
        total_written += (size_t)res;
    }

    if (close(fd) < 0) {
        fprintf(stderr, "error: closing '%s': %s\n", out_path, strerror(errno));
        return -1;
    }

    return 0;
}


static int decode(img_t **frame_ptr, const char *in_path) {
    img_t *frame = *frame_ptr;
    tjhandle dec = tj3Init(TJINIT_DECOMPRESS);
    if (!dec) {
        fprintf(stderr, "error: tj3Init: %s\n", err_str(NULL));
        clean(frame_ptr, &dec, NULL);
        return -1;
    }

    if (tj3DecompressHeader(dec, frame->jpeg_buffer, frame->jpeg_size) != 0) {
        fprintf(stderr, "error: '%s' is not a readable JPEG: %s\n", in_path, err_str(dec));
        clean(frame_ptr, &dec, NULL);
        return -1;
    }

    frame->w = tj3Get(dec, TJPARAM_JPEGWIDTH);
    frame->h = tj3Get(dec, TJPARAM_JPEGHEIGHT);

    frame->rgb = tj3Alloc((size_t)frame->w * frame->h * 3);
    if (!frame->rgb) {
        fprintf(stderr, "error: tj3Alloc\n");
        clean(frame_ptr, &dec, NULL);
        return -1;
    }

    if (tj3Decompress8(dec, frame->jpeg_buffer, frame->jpeg_size, frame->rgb, frame->w * 3, TJPF_RGB) != 0) {
        fprintf(stderr, "error: decode: %s\n", err_str(dec));
        clean(frame_ptr, &dec, NULL);
        return -1;
    }

    tj3Destroy(dec);
    return 0;
}


static int encode(img_t **frame_ptr, uint8_t **out_jpeg, size_t *out_jpeg_size) {
    img_t *frame = *frame_ptr;
    tjhandle enc = tj3Init(TJINIT_COMPRESS);
    if (!enc) {
        fprintf(stderr, "error: tj3Init: %s\n", err_str(NULL));
        clean(frame_ptr, &enc, NULL);
        return -1;
    }

    if (tj3Set(enc, TJPARAM_QUALITY, JPEG_QUALITY) != 0 ||
        tj3Set(enc, TJPARAM_SUBSAMP, TJSAMP_420) != 0) {
        fprintf(stderr, "error: setting encoder params: %s\n", err_str(enc));
        clean(frame_ptr, &enc, NULL);
        return -1;
    }

    if (tj3Compress8(enc, frame->rgb, frame->w, frame->w * 3, frame->h, TJPF_RGB,
                     out_jpeg, out_jpeg_size) != 0) {
        fprintf(stderr, "error: encode: %s\n", err_str(enc));
        clean(frame_ptr, &enc, *out_jpeg);
        return -1;
    }

    tj3Destroy(enc);
    return 0;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <filename.jpeg>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *in_path = argv[1];
    char out_path[PATH_MAX];
    if (build_output_path(in_path, out_path, sizeof(out_path))) {
        fprintf(stderr, "error: output path too long for input '%s'\n", in_path);
        return EXIT_FAILURE;
    }

    /* ----- Read image to memory ----- */
    size_t jpeg_size = 0;
    uint8_t *jpeg_buffer = read_file(in_path, &jpeg_size);
    if (!jpeg_buffer)
        return EXIT_FAILURE;

    /* ----- Decode ----- */
    img_t *frame = (img_t *) malloc(sizeof(img_t));
    if (!frame) {
        free(jpeg_buffer);
        fprintf(stderr, "error: frame malloc\n");
        return EXIT_FAILURE;
    }

    frame->jpeg_size = jpeg_size;
    frame->jpeg_buffer = jpeg_buffer;
    frame->rgb = NULL;
    if (decode(&frame, in_path))
        return EXIT_FAILURE;

    /* ----- Encode ----- */
    uint8_t *out_jpeg = NULL;
    size_t out_jpeg_size = 0;
    if (encode(&frame, &out_jpeg, &out_jpeg_size))
        return EXIT_FAILURE;

    /* ----- Write to new file ----- */
    if (write_file(out_path, out_jpeg, out_jpeg_size)) {
        clean(&frame, NULL, out_jpeg);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "wrote: %s (%dx%d, %zu bytes)\n", out_path, frame->w, frame->h, out_jpeg_size);
    printf("%s\n", out_path);

    clean(&frame, NULL, out_jpeg);
    return EXIT_SUCCESS;
}
