#include <stdio.h>
#include <dlfcn.h>
#include "hpjbig_wrapper.h"

int (*hp_encode_bits_to_jbig) (int iWidth, int iHeight, unsigned char **pBuff,
                        HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt);
int (*hp_init_lib) (int iFlag);

int main(int argc, char *argv[])
{
    void *libhdl = NULL;
    
    libhdl = dlopen(argv[1], RTLD_NOW);
    if(!libhdl) {
        printf("Could not load %s\n", argv[1]);
        return 1;
    }
    *(void **) (&hp_encode_bits_to_jbig) = dlsym(libhdl, "hp_encode_bits_to_jbig");
    *(void **) (&hp_init_lib) = dlsym(libhdl, "hp_init_lib");
    return 0;
}
