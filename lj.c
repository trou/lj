#include <jbig.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "hpjbig_wrapper.h"

int hp_init_lib(int iFlags)
{
    return 0;
}

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

#define LTEST_W             colidx > 0
#define LVAL_W(colidx)      cur_row[colidx-1]
#define LTEST_NW            colidx > 0
#define LVAL_NW(colidx)     seedrow[colidx-1]
#define LTEST_WW            colidx > 1
#define LVAL_WW(colidx)     cur_row[colidx-2]
#define LTEST_NWW           colidx > 1
#define LVAL_NWW(colidx)    seedrow[colidx-2]
#define LTEST_NE            (colidx+1) < uiLogicalImageWidth
#define LVAL_NE(colidx)     seedrow[colidx+1]
#define LTEST_NEWCOL        1
#define LVAL_NEWCOL(colidx) new_color
#define LTEST_CACHE         1
#define LVAL_CACHE(colidx)  cache

#define LOC1TEST      LTEST_NE
#define LOC1VAL(colidx)  LVAL_NE(colidx)
#define LOC2TEST      LTEST_NW
#define LOC2VAL(colidx)  LVAL_NW(colidx)
#define LOC3TEST      LTEST_NEWCOL
#define LOC3VAL(colidx)  LVAL_NEWCOL(colidx)

int HPJetReadyCompress(unsigned char   *pCompressedData,
                   uint32_t        *pCompressedDataLen,
                   unsigned char   *InputData,
                   const uint32_t  uiLogicalImageWidth,
                   const uint32_t  uiLogicalImageHeight)
{
    int result;
    unsigned char *seedrow;
    uint8_t *cur_row;
    uint8_t *pCompressedDataEnd = &pCompressedData[*pCompressedDataLen];
    int colidx, seedrow_count, coldata_idx, location, run_count;
    uint8_t new_color[3], cache[3];

    if(InputData)
    {
        seedrow = malloc(3*uiLogicalImageWidth);
        if(seedrow)
        {
            memset(seedrow, 0xFF, 3*uiLogicalImageWidth);
            memset(new_color, 0, 3);
            memset(cache, 0xFF, 3);

            for(int lineidx = 0; lineidx < uiLogicalImageHeight; lineidx++) {
                cur_row = &InputData[3*uiLogicalImageWidth*lineidx];
                colidx = 0;
                coldata_idx = 0;
                while(colidx < uiLogicalImageWidth) {
                    seedrow_count = 0;
                    while(colidx < uiLogicalImageWidth &&
                          cur_row[coldata_idx] == seedrow[coldata_idx] &&
                          cur_row[coldata_idx + 1] == seedrow[coldata_idx + 1] &&
                          cur_row[coldata_idx + 2] == seedrow[coldata_idx + 2]) {
                        seedrow_count++;
                        colidx++;
                        coldata_idx += 3;
                    }

                    if(colidx == uiLogicalImageWidth) {
                        pCompressedData = encode_seedcmd(pCompressedData, pCompressedDataEnd, seedrow_count);
                        break;
                    }

                    if(colidx + 1 >= uiLogicalImageWidth ||
                        memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx+3], 3)) {
                        if(colidx && !memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx-3], 3)) {
                            location = 1;
                        } else {
                            if (memcmp(&cur_row[coldata_idx], cache, 3)) {
                                location = 0;
                                memcpy(new_color, &cur_row[coldata_idx], 3);
                            } else {
                                location = 3;
                            }
                       }
                    } else {
                        location = 2;
                    }

                    /* Looking for runs */
                    if(colidx + 1 >= uiLogicalImageWidth ||
                        memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx+3], 3)) {
                        /* No run found */
                        run_count = 0;
                        uint8_t *color_ptr = &cur_row[coldata_idx+3];

                        coldata_idx += 3;
                        colidx++;
                        while(colidx+1 < uiLogicalImageWidth &&
                              !memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx+3], 3) &&
                              !memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx], 3)) {
                            // TODO: make sure the while check is sound
                            run_count++;
                            colidx++;
                            coldata_idx += 3;
                        }
                        pCompressedData = encode_literal(pCompressedData, pCompressedDataEnd, color_ptr,
                                                         location, seedrow_count, run_count, new_color);

                    } else {
                    }

                }
            }
        } else {
            result = -2;
        }
    }
    return result;
}
