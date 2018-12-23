#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "hpjbig_wrapper.h"

uint32_t
rc_crc32(uint32_t crc, const unsigned char *buf, size_t len)
{
    static uint32_t table[256];
    static int have_table = 0;
    uint32_t rem;
    uint8_t octet;
    int i, j;
    const unsigned char *p, *q;

    /* This check is not thread safe; there is no mutex. */
    if (have_table == 0) {
        /* Calculate CRC table. */
        for (i = 0; i < 256; i++) {
            rem = i;  /* remainder from polynomial division */
            for (j = 0; j < 8; j++) {
                if (rem & 1) {
                    rem >>= 1;
                    rem ^= 0xedb88320;
                } else
                    rem >>= 1;
            }
            table[i] = rem;
        }
        have_table = 1;
    }

    crc = ~crc;
    q = buf + len;
    for (p = buf; p < q; p++) {
        octet = *p;  /* Cast to unsigned octet. */
        crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
    }
    return ~crc;
}

int (*HPJetReadyCompress) (uint8_t     *pCompressedData,
                           uint32_t    *pCompressedDataLen,
                           uint8_t     *InputData,
                           const uint32_t    uiLogicalImageWidth,
                           const uint32_t    uiLogicalImageHeight);

int main(int argc, char *argv[])
{
    void *libhdl = NULL;
    int input_fd = 0;
    int out_fd = 0;
    int res = 0;
    uint8_t *input_mmap = NULL;
    uint8_t *output_buffer;
    uint32_t out_buffer_size;
    struct stat input_stat;
    int width = 0, height = 0;

    if(argc < 5) {
        printf("Usage: test lib.so input_file width height\n");
        return 1;
    }

    width = atoi(argv[3]);
    height = atoi(argv[4]);
    printf("Width: %d, Height: %d\n", width, height);

    libhdl = dlopen(argv[1], RTLD_NOW);
    if(!libhdl) {
        printf("Could not load %s\n", argv[1]);
        return 1;
    }

    // mmap input file
    input_fd = open(argv[2], O_RDONLY);
    if(input_fd == -1) {
        printf("Could not open %s\n", argv[2]);
        return 1;
    }
    fstat(input_fd, &input_stat);

    printf("Input size: %lu\n", input_stat.st_size);
    if(input_stat.st_size < (width*height*3)) {
        printf("Input file too small for given image size\n");
        return 1;
    }

    input_mmap = mmap(NULL, input_stat.st_size, PROT_READ, MAP_PRIVATE,
                      input_fd, 0);
    if(input_mmap == MAP_FAILED) {
        perror("Could not mmap input file");
        return 1;
    }


    *(void **) (&HPJetReadyCompress) = dlsym(libhdl, "HPJetReadyCompress");
    if(HPJetReadyCompress == NULL) {
        printf("Could not resolve HPJetReadyCompress: %s\n", dlerror());
        return 1;
    }

    output_buffer = (uint8_t *) malloc(input_stat.st_size);
    out_buffer_size = input_stat.st_size;
    if(output_buffer == NULL) {
        perror("Could not allocate output buffer\n");
        return 1;
    }
    printf("HPJetReadyCompress return value: ");
    res = HPJetReadyCompress(output_buffer, &out_buffer_size, input_mmap, width, height);
    printf("%d\n", res);
    printf("out_buffer_size = %u\n", out_buffer_size);
    printf("CRC32: %08x\n", rc_crc32(0, output_buffer, out_buffer_size));

    if(out_buffer_size < 0x80000000) {
        out_fd = open(argv[5], O_RDWR|O_TRUNC|O_CREAT, 0600);
        write(out_fd, output_buffer, out_buffer_size);
        close(out_fd);
    }

    free(output_buffer);
    munmap(input_mmap, input_stat.st_size);
    close(input_fd);


    return 0;
}
