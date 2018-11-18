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

uint8_t *encode_seedcmd(uint8_t *outptr, uint8_t *pastoutmem, int repl_cnt)
{
    uint8_t byte;
    uint8_t *result;

    if(repl_cnt < 3) {
        byte = 0x80 | (8*repl_cnt);
    } else {
        byte = 0x98;
    }
    result = write_comp_byte(byte, outptr, pastoutmem);

    if(!result)
        return NULL;
    if(repl_cnt > 2) {
        char i;

        for(i=repl_cnt-3; i > -2; i -= 3)
            *result++ = 0xFF;
        *result++ = (uint8_t) i;
    }
    
    return result;
}

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
    uint8_t *pCompressedDataOrig = pCompressedData;
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
                        /* Got a run ?*/
                        run_count = 0;
                        colidx++;
                        coldata_idx += 3;
                        while(colidx+1 < uiLogicalImageWidth &&
                              !memcmp(&cur_row[coldata_idx], &cur_row[coldata_idx+3], 3)) {
                            run_count++;
                            coldata_idx += 3;
                            coldata_idx++;
                        }
                        coldata_idx += 3;
                        coldata_idx++;
                        pCompressedData = encode_runcmd(pCompressedData, pCompressedDataEnd,
                                                        location, seedrow_count, run_count, new_color);
                    }
                    memcpy(cache, &cur_row[coldata_idx-3], 3);
                }
                seedrow = &InputData[3*uiLogicalImageWidth*lineidx];
            }
            if(seedrow)
                free(seedrow);
            if(pCompressedData <= pCompressedDataEnd) {
                *pCompressedDataLen = pCompressedDataOrig - pCompressedData;
                result = 0;
            } else {
                *pCompressedDataLen = 0;
                result = -1;
            }

        } else {
            result = -2;
        }
    } else {
        /* No input data */
        for(int i=0; i < uiLogicalImageHeight; i++) {
            pCompressedData = encode_seedcmd(pCompressedData, pCompressedDataEnd, uiLogicalImageWidth);
        }
        *pCompressedDataLen = pCompressedDataOrig - pCompressedData;
        result = 0;
    }
    return result;
}
