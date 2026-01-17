/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stddef.h>

#define UNISTRAP_VERSION "0.0.1"
#define SECTOR_SIZE      0x200
#define MBR_END_OFFSET   SECTOR_SIZE

#define ALIGN_UP(value, align)        (((value) + (align)-1) & ~((align)-1))

/* Run-time parameters */
static const char *output_file = "kernel.img";
static const char *bootstrap_path = NULL;
static const char *kernel_path = NULL;

/*
 * Represents the image header that exists
 * on top of the payloads
 *
 * @hdr_size:       Size of header
 * @sector_count:   Total number of sectors in image
 * @bootstrap_off:  Offset of bootstrap payload
 * @bootstrap_size: Size of bootstrap payload
 * @kernel_off:     Offset of kernel payload
 * @kernel_size:    size of kernel payload
 */
struct image_header {
    uint16_t hdr_size;
    uint16_t sector_count;
    off_t bootstrap_off;
    size_t bootstrap_size;
    off_t kernel_off;
    size_t kernel_size;
} __attribute__((packed));

static void
help(void)
{
    printf(
        "unistrap - mbr kernel imager\n"
        "----------------------------\n"
        "[-h]   Display this help menu\n"
        "[-v]   Display unistrap version\n"
        "[-o]   Image output path\n"
        "[-b]   Bootstrap image path\n"
        "[-k]   Kernel image path\n"
    );
}

static void
version(void)
{
    printf(
        "-------------------------------\n"
        "Copyright (c) 2026, Ian Moffett\n"
        "Unistrap v%s\n"
        "-------------------------------\n",
        UNISTRAP_VERSION
    );
}

static int
output_append(int out_fd, int in_fd, size_t sz)
{
    char *data;

    data = mmap(
        NULL,
        sz,
        PROT_READ,
        MAP_SHARED,
        in_fd,
        0
    );

    if (data == NULL) {
        perror("mmap");
        return -1;
    }

    write(out_fd, data, sz);
    munmap(data, sz);
    return 0;
}

static int
generate(void)
{
    char pad[512];
    struct image_header hdr;
    size_t bs_sz, k_sz, total_sz;
    size_t align_sz, pad_sz;
    int out_fd;
    int k_fd, bs_fd;

    bs_fd = open(bootstrap_path, O_RDONLY);
    if (bs_fd < 0) {
        perror("open[bs]");
        return -1;
    }

    k_fd = open(kernel_path, O_RDONLY);
    if (k_fd < 0) {
        close(bs_fd);
        return -1;
    }

    out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd < 0) {
        perror("open[out]");
        close(bs_fd);
        close(k_fd);
        return -1;
    }

    hdr.hdr_size = sizeof(hdr);

    /* Initialize the bootstrap part of header */
    bs_sz = lseek(bs_fd, 0, SEEK_END);
    lseek(bs_fd, 0, SEEK_SET);
    hdr.bootstrap_size = bs_sz;
    hdr.bootstrap_off = MBR_END_OFFSET;

    /* Initialize the kernel part of header */
    k_sz = lseek(k_fd, 0, SEEK_END);
    lseek(k_fd, 0, SEEK_SET);
    hdr.bootstrap_size = k_sz;
    hdr.bootstrap_off = MBR_END_OFFSET + bs_sz;

    /* Prepare the header size */
    hdr.sector_count = hdr.hdr_size + k_sz + bs_sz;
    hdr.sector_count = ALIGN_UP(hdr.sector_count, SECTOR_SIZE);
    hdr.sector_count /= SECTOR_SIZE;

    write(out_fd, &hdr, sizeof(hdr));
    if (output_append(out_fd, bs_fd, bs_sz) < 0) {
        close(out_fd);
        close(bs_fd);
        close(k_fd);
        return -1;
    }

    if (output_append(out_fd, k_fd, k_sz) < 0) {
        close(out_fd);
        close(bs_fd);
        close(k_fd);
        return -1;
    }

    total_sz = k_sz + bs_sz + sizeof(hdr);
    align_sz = ALIGN_UP(total_sz, SECTOR_SIZE);
    pad_sz = align_sz - total_sz;

    if (pad_sz > 0) {
        memset(pad, 0xEE, pad_sz);
        write(out_fd, pad, pad_sz);
    }

    printf(
        "[*] Wrote %zu bytes, padded to %zu bytes\n",
        total_sz,
        pad_sz
    );

    close(out_fd);
    close(bs_fd);
    close(k_fd);
    return 0;
}

int
main(int argc, char **argv)
{
    int opt;

    if (argc < 2) {
        printf("fatal: too few arguments\n");
        help();
        return -1;
    }

    while ((opt = getopt(argc, argv, "hvo:b:k:")) != -1) {
        switch (opt) {
        case 'h':
            help();
            return -1;
        case 'v':
            version();
            return -1;
        case 'o':
            output_file = strdup(optarg);
            break;
        case 'b':
            bootstrap_path = strdup(optarg);
            break;
        case 'k':
            kernel_path = strdup(optarg);
            break;
        }
    }

    if (bootstrap_path == NULL) {
        printf("fatal: expected bootstrap path\n");
        help();
        return -1;
    }

    if (kernel_path == NULL) {
        printf("fatal: expected kernel path\n");
        help();
        return -1;
    }

    return generate();
}
