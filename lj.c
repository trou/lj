#include <stdio.h>
#include <jbig.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "hpjbig_wrapper.h"

unsigned char *orig = NULL;
int verbose = 1;


void hexdump(char *prefix, uint8_t *buffer, int len)
{
    if(verbose) printf("%s", prefix);
    for(int i=0; i<len; i++)
        if(verbose) printf(" %02X", buffer[i]);
    if(verbose) printf("\n");
}

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

/* Reimplemented macro as function */
uint8_t *write_comp_byte(uint8_t val, uint8_t *outptr, uint8_t *pastoutmem)

{
    if(verbose) printf("write_comp_byte[%lx]: %d / 0x%02x\n", outptr-orig, val, val);
    if(outptr >= pastoutmem)
        return NULL;
    *outptr++ = val;
    return outptr;
}

uint8_t *encode_count(int count, int over, uint8_t *outptr, uint8_t *pastoutmem)
{
    if(verbose) printf("encode_count(%d, %d)\n", count, over);
    /* TODO: verify it's sound */
    if(count >= over) {
        count -= over;
        if(count <= 253) {
            outptr = write_comp_byte(count, outptr, pastoutmem);
        } else if(count <= (254+255)) {
            if(outptr + 1 >= pastoutmem)
                return NULL;
            outptr = write_comp_byte(0xFE, outptr, pastoutmem);
            outptr = write_comp_byte(count-0xFE, outptr, pastoutmem);
        } else {
            if(outptr + 2 >= pastoutmem)
                return NULL;
            count -= 0xFF;
            if(count > 0xFFFF)
                return NULL;
            outptr = write_comp_byte(0xFF, outptr, pastoutmem);
            outptr = write_comp_byte(count >> 8, outptr, pastoutmem);
            outptr = write_comp_byte((count&0xFF), outptr, pastoutmem);
        }
    }
    if(verbose) printf("encode_count end\n");
    return outptr;
}

uint8_t *encode_count2(int count, int over, uint8_t *outptr, uint8_t *pastoutmem)
{
    if(verbose) printf("encode_count2(%d, %d)\n", count, over);
    if (count >= over) {
        count -= over;
        while(count > 0xFE) {
            count -= 0xFF;
            outptr = write_comp_byte(0xFF, outptr, pastoutmem);
            if(!outptr) return NULL;
        }
        outptr = write_comp_byte((count & 0xFF), outptr, pastoutmem);
        if(!outptr)
            return NULL;
    }
    if(verbose) printf("encode_count2 end\n");
    return outptr;
}

uint8_t *encode_seedcmd(uint8_t *outptr, uint8_t *pastoutmem, int repl_cnt)
{
    uint8_t byte;

    if(verbose) printf("encode_seedcmd(%d)\n", repl_cnt);
    if(repl_cnt < 3) {
        byte = 0x80 | (8*repl_cnt);
    } else {
        byte = 0x98;
    }

    outptr = write_comp_byte(byte, outptr, pastoutmem);

    if(!outptr)
        return NULL;

    outptr = encode_count2(repl_cnt, 3, outptr, pastoutmem);
    if(verbose) printf("encode_seedcmd end\n");
    return outptr;
}

uint8_t *encode_runcmd(uint8_t *outptr, uint8_t *pastoutmem, int location, unsigned int seedrow_count, unsigned int run_count, uint8_t *new_color) {
    uint8_t byte;

    if(verbose) printf("encode_runcmd(%d, %u, %u)\n", location, seedrow_count, run_count);
    if ( seedrow_count <= 2 )
        byte = 8 * seedrow_count | 32 * location | 0x80;
    else
        byte = 32 * location | 0x98;

    if(run_count <= 6)
        byte |= run_count;
    else
        byte |= 7;

    outptr = write_comp_byte(byte, outptr, pastoutmem);
    if(!outptr)
        return NULL;

    outptr = encode_count2(seedrow_count, 3, outptr, pastoutmem);
    if(!outptr)
        return NULL;

    /* if required, write out color of first pixel */
    if(location == 0) {
        outptr = write_comp_byte(new_color[0], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(new_color[1], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(new_color[2], outptr, pastoutmem);
        if(!outptr) return NULL;
    }

    outptr = encode_count2(run_count, 7, outptr, pastoutmem);
    return outptr;
}

uint8_t *encode_literal(uint8_t *outptr, uint8_t *pastoutmem,
                        uint8_t *color_ptr, int location,
                        unsigned int seedrow_count, unsigned int run_count,
                        uint8_t *new_color)
{
    uint8_t byte;

    if(verbose) printf("encode_literal(%d, %u, %u)\n", location, seedrow_count, run_count);
    hexdump("new_color:", new_color, 3);

    byte = 32 * location;
    if (seedrow_count <= 2)
        byte = 8 * seedrow_count | byte;
    else
        byte = byte | 0x18;

    if (run_count <= 6)
        byte = run_count | byte;
    else
        byte = byte | 7;

    outptr = write_comp_byte(byte, outptr, pastoutmem);
    if(!outptr)
        return NULL;

    outptr = encode_count2(seedrow_count, 3, outptr, pastoutmem);
    if(!outptr)
        return NULL;

    if(location == 0) {
        outptr = write_comp_byte(new_color[0], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(new_color[1], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(new_color[2], outptr, pastoutmem);
        if(!outptr) return NULL;
    }

    if(run_count >= 7) {
        for(int i=0; i <= 6; i++) {
            outptr = write_comp_byte(color_ptr[0], outptr, pastoutmem);
            if(!outptr) return NULL;
            outptr = write_comp_byte(color_ptr[1], outptr, pastoutmem);
            if(!outptr) return NULL;
            outptr = write_comp_byte(color_ptr[2], outptr, pastoutmem);
            if(!outptr) return NULL;
            color_ptr += 3;
        }

        run_count -= 7;
        while(run_count > 0xFE) {
            run_count -= 0xFF;
            outptr = write_comp_byte(0xFF, outptr, pastoutmem);
            if(!outptr) return NULL;
        }
        outptr = write_comp_byte((run_count & 0xFF), outptr, pastoutmem);
        // outptr = encode_count(run_count, 7, outptr, pastoutmem);
        //run_count %= 7;
        // TODO: figure out the run_count encoding loop with memcpy(color_ptr)
        if(!outptr)
            return NULL;
    }
    for(int i=0; i < run_count; i++) {
        outptr = write_comp_byte(color_ptr[0], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(color_ptr[1], outptr, pastoutmem);
        if(!outptr) return NULL;
        outptr = write_comp_byte(color_ptr[2], outptr, pastoutmem);
        if(!outptr) return NULL;
        color_ptr += 3;
    }

    if(verbose) printf("encode_literal end\n");
    return outptr;
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

    orig = pCompressedData;

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
                    if(verbose) printf("colidx: %x, coldata_idx: %x, lineidx=%x\n", colidx, coldata_idx, lineidx);
                    hexdump("currow:", &cur_row[coldata_idx], 10);
                    hexdump("seedrow:", &seedrow[coldata_idx], 10);
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
                        if(colidx && !memcmp(&cur_row[coldata_idx], &cur_row[coldata_idx-3], 3)) {
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

                    if(verbose) printf("location: %d\n", location);
                    if(verbose) printf("coldata_idx: %x\n", coldata_idx);
                    hexdump("currow:", &cur_row[coldata_idx], 10);
                    hexdump("seedrow:", &seedrow[coldata_idx], 10);
                    /* Looking for runs */
                    if(colidx + 1 >= uiLogicalImageWidth ||
                        memcmp(&cur_row[coldata_idx], &cur_row[coldata_idx+3], 3)) {
                        printf("no run!\n");
                        /* No run found */
                        run_count = 0;
                        uint8_t *color_ptr = &cur_row[coldata_idx+3];

                        coldata_idx += 3;
                        colidx++;
                        while(colidx+1 <= uiLogicalImageWidth &&
                              memcmp(&cur_row[coldata_idx], &cur_row[coldata_idx+3], 3) &&
                              memcmp(&cur_row[coldata_idx], &seedrow[coldata_idx], 3)) {
                            // TODO: make sure the while check is sound
                            run_count++;
                            colidx++;
                            coldata_idx += 3;
                        }
                        pCompressedData = encode_literal(pCompressedData, pCompressedDataEnd, color_ptr,
                                                         location, seedrow_count, run_count, new_color);

                    } else {
                        /* Got a run ?*/
                        if(verbose) printf("run: colidx=%x!\n", colidx);
                        run_count = 0;
                        colidx++;
                        coldata_idx += 3;
                        while(colidx+1 < uiLogicalImageWidth &&
                              !memcmp(&cur_row[coldata_idx], &cur_row[coldata_idx+3], 3)) {
                            run_count++;
                            coldata_idx += 3;
                            colidx++;
                        }
                        coldata_idx += 3;
                        colidx++;
                        pCompressedData = encode_runcmd(pCompressedData, pCompressedDataEnd,
                                                        location, seedrow_count, run_count, new_color);
                    }
                    memcpy(cache, &cur_row[coldata_idx-3], 3);
                }
                memcpy(seedrow, &InputData[3*uiLogicalImageWidth*lineidx], 3*uiLogicalImageWidth);
            }
            if(seedrow)
                free(seedrow);
            if(pCompressedData <= pCompressedDataEnd) {
                *pCompressedDataLen =  pCompressedData-pCompressedDataOrig;
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
        *pCompressedDataLen = pCompressedData - pCompressedDataOrig ;
        result = 0;
    }
    return result;
}
