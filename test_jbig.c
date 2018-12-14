#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include "hpjbig_wrapper.h"

int (*hp_encode_bits_to_jbig) (int iWidth, int iHeight, unsigned char **pBuff,
                        HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt);
int (*hp_init_lib) (int iFlag);

int main(int argc, char *argv[])
{
    void *libhdl = NULL;
    int input_fd = 0;
    int res = 0;
    uint8_t *input_mmap = NULL;
    HPLJZjcBuff output_buffer;
    HPLJZjsJbgEncSt state;
    struct stat input_stat;
    int width = 0, height = 0;

    if(argc < 5) {
        printf("Usage: test lib.so input_file width height [output]\n");
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


    *(void **) (&hp_encode_bits_to_jbig) = dlsym(libhdl, "hp_encode_bits_to_jbig");
    if(hp_encode_bits_to_jbig == NULL) {
        printf("Could not resolve hp_encode_bits_to_jbig: %s\n", dlerror());
        return 1;
    }
    *(void **) (&hp_init_lib) = dlsym(libhdl, "hp_init_lib");
    if(hp_init_lib == NULL) {
        printf("Could not resolve hp_init_lib: %s\n", dlerror());
        return 1;
    }

    printf("Initializing library...\n");
    if(setenv("DEVICE_URI", "hp://test", 0)) {
        printf("Could not set DEVICE_URI env variable\n");
        return 1;
    }
    hp_init_lib(0);

    output_buffer.pszCompressedData = (uint8_t *) malloc(input_stat.st_size*2);
    output_buffer.dwTotalSize = 0;
    if(output_buffer.pszCompressedData == NULL) {
        perror("Could not allocate output buffer\n");
        return 1;
    }
    memset(output_buffer.pszCompressedData, 0, input_stat.st_size*2);
    printf("Encoding data: ");
    res = hp_encode_bits_to_jbig(width, height, &input_mmap, &output_buffer,
                           &state);
    printf("%d\n", res);
    printf("dwTotalSize = %lu\n", output_buffer.dwTotalSize);

    if(argc == 6) {
        FILE *result = fopen(argv[5], "wb+");
        if(!result)
        {
            perror("Could not open output file\n");
            return -1;
        }
        fwrite(output_buffer.pszCompressedData, 1, output_buffer.dwTotalSize, result);
        fclose(result);
    }

    return 0;
}
