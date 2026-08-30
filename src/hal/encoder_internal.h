#ifndef HG_HAL_ENCODER_INTERNAL_H
#define HG_HAL_ENCODER_INTERNAL_H

#include <stdint.h>

/* Full-step quadrature state table (Ben Buxton style). One entry per detent,
 * quiet against bounce/half-steps unlike a naive edge counter. */
#define HG_ENC_R_START     0x0
#define HG_ENC_R_CW_FINAL  0x1
#define HG_ENC_R_CW_BEGIN  0x2
#define HG_ENC_R_CW_NEXT   0x3
#define HG_ENC_R_CCW_BEGIN 0x4
#define HG_ENC_R_CCW_FINAL 0x5
#define HG_ENC_R_CCW_NEXT  0x6
#define HG_ENC_DIR_CW   0x10
#define HG_ENC_DIR_CCW  0x20

static const uint8_t hg_encoder_table[7][4] = {
    /* R_START */
    {HG_ENC_R_START, HG_ENC_R_CW_BEGIN, HG_ENC_R_CCW_BEGIN, HG_ENC_R_START},
    /* R_CW_FINAL */
    {HG_ENC_R_CW_NEXT, HG_ENC_R_START, HG_ENC_R_CW_FINAL, HG_ENC_R_START | HG_ENC_DIR_CW},
    /* R_CW_BEGIN */
    {HG_ENC_R_CW_NEXT, HG_ENC_R_CW_BEGIN, HG_ENC_R_START, HG_ENC_R_START},
    /* R_CW_NEXT */
    {HG_ENC_R_CW_NEXT, HG_ENC_R_CW_BEGIN, HG_ENC_R_CW_FINAL, HG_ENC_R_START},
    /* R_CCW_BEGIN */
    {HG_ENC_R_CCW_NEXT, HG_ENC_R_START, HG_ENC_R_CCW_BEGIN, HG_ENC_R_START},
    /* R_CCW_FINAL */
    {HG_ENC_R_CCW_NEXT, HG_ENC_R_CCW_FINAL, HG_ENC_R_START, HG_ENC_R_START | HG_ENC_DIR_CCW},
    /* R_CCW_NEXT */
    {HG_ENC_R_CCW_NEXT, HG_ENC_R_CCW_FINAL, HG_ENC_R_CCW_BEGIN, HG_ENC_R_START},
};

#endif
