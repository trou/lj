#include <jbig.h>
#include "hpjbig_wrapper.h"

void hpOutputBie(unsigned char *start, size_t len, void *file)
{
}

int hp_encode_bits_to_jbig(int iWidth, int iHeight, unsigned char **pBuff, HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt)
{
    struct jbg_enc_state *s = NULL;

    jbg_enc_init(s, iWidth, iHeight, 1, pBuff, hpOutputBie, pOutBuff);
    return 0;
}
