#include <jbig.h>
#include <string.h>
#include "hpjbig_wrapper.h"

void hpOutputBie(unsigned char *start, size_t len, void *output)
{
    HPLJZjcBuff *outbuf = (HPLJZjcBuff *)output;
    memcpy(&outbuf->pszCompressedData[outbuf->dwTotalSize],
           start, len);
    outbuf->dwTotalSize += len;
}

int hp_encode_bits_to_jbig(int iWidth, int iHeight, unsigned char **pBuff, HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt)
{
    struct jbg_enc_state s;

    /* Encode output */
    jbg_enc_init(&s, iWidth, iHeight, 1, pBuff, hpOutputBie, pOutBuff);
    jbg_enc_options(&s, JBG_SMID|JBG_ILEAVE,
                    JBG_LRLTWO|JBG_TPDON|JBG_TPBON|JBG_DPON,
                    0x80, 0, 0);
    jbg_enc_out(&s);
    jbg_enc_free(&s);

    /* Copy encoder state to wrapper */
    pJbgEncSt->d = s.d;
    pJbgEncSt->xd = s.xd;
    pJbgEncSt->dl = s.dl;
    pJbgEncSt->l0 = s.l0;
    pJbgEncSt->order = s.order;
    pJbgEncSt->planes = s.planes;
    pJbgEncSt->options = s.options;
    pJbgEncSt->mx = s.mx;
    pJbgEncSt->my = s.my;

    return 0;
}
