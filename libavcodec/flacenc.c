/**
 * FLAC audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "bitstream.h"
#include "crc.h"
#include "golomb.h"
#include "lls.h"

#define FLAC_MAX_CH  8
#define FLAC_MIN_BLOCKSIZE  16
#define FLAC_MAX_BLOCKSIZE  65535

#define FLAC_SUBFRAME_CONSTANT  0
#define FLAC_SUBFRAME_VERBATIM  1
#define FLAC_SUBFRAME_FIXED     8
#define FLAC_SUBFRAME_LPC      32

#define FLAC_CHMODE_NOT_STEREO      0
#define FLAC_CHMODE_LEFT_RIGHT      1
#define FLAC_CHMODE_LEFT_SIDE       8
#define FLAC_CHMODE_RIGHT_SIDE      9
#define FLAC_CHMODE_MID_SIDE       10

#define ORDER_METHOD_EST     0
#define ORDER_METHOD_2LEVEL  1
#define ORDER_METHOD_4LEVEL  2
#define ORDER_METHOD_8LEVEL  3
#define ORDER_METHOD_SEARCH  4
#define ORDER_METHOD_LOG     5

#define FLAC_STREAMINFO_SIZE  34

#define MIN_LPC_ORDER       1
#define MAX_LPC_ORDER      32
#define MAX_FIXED_ORDER     4
#define MAX_PARTITION_ORDER 8
#define MAX_PARTITIONS     (1 << MAX_PARTITION_ORDER)
#define MAX_LPC_PRECISION  15
#define MAX_LPC_SHIFT      15
#define MAX_RICE_PARAM     14

typedef struct CompressionOptions {
    int compression_level;
    int block_time_ms;
    int use_lpc;
    int lpc_coeff_precision;
    int min_prediction_order;
    int max_prediction_order;
    int prediction_order_method;
    int min_partition_order;
    int max_partition_order;
} CompressionOptions;

typedef struct RiceContext {
    int porder;
    int params[MAX_PARTITIONS];
} RiceContext;

typedef struct FlacSubframe {
    int type;
    int type_code;
    int obits;
    int order;
    int32_t coefs[MAX_LPC_ORDER];
    int shift;
    RiceContext rc;
    int32_t samples[FLAC_MAX_BLOCKSIZE];
    int32_t residual[FLAC_MAX_BLOCKSIZE];
} FlacSubframe;

typedef struct FlacFrame {
    FlacSubframe subframes[FLAC_MAX_CH];
    int blocksize;
    int bs_code[2];
    uint8_t crc8;
    int ch_mode;
} FlacFrame;

typedef struct FlacEncodeContext {
    PutBitContext pb;
    int channels;
    int ch_code;
    int samplerate;
    int sr_code[2];
    int blocksize;
    int max_framesize;
    uint32_t frame_count;
    FlacFrame frame;
    CompressionOptions options;
    AVCodecContext *avctx;
} FlacEncodeContext;

static const int flac_samplerates[16] = {
    0, 0, 0, 0,
    8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000,
    0, 0, 0, 0
};

static const int flac_blocksizes[16] = {
    0,
    192,
    576, 1152, 2304, 4608,
    0, 0,
    256, 512, 1024, 2048, 4096, 8192, 16384, 32768
};

/**
 * Writes streaminfo metadata block to byte array
 */
static void write_streaminfo(FlacEncodeContext *s, uint8_t *header)
{
    PutBitContext pb;

    memset(header, 0, FLAC_STREAMINFO_SIZE);
    init_put_bits(&pb, header, FLAC_STREAMINFO_SIZE);

    /* streaminfo metadata block */
    put_bits(&pb, 16, s->blocksize);
    put_bits(&pb, 16, s->blocksize);
    put_bits(&pb, 24, 0);
    put_bits(&pb, 24, s->max_framesize);
    put_bits(&pb, 20, s->samplerate);
    put_bits(&pb, 3, s->channels-1);
    put_bits(&pb, 5, 15);       /* bits per sample - 1 */
    flush_put_bits(&pb);
    /* total samples = 0 */
    /* MD5 signature = 0 */
}

/**
 * Sets blocksize based on samplerate
 * Chooses the closest predefined blocksize >= BLOCK_TIME_MS milliseconds
 */
static int select_blocksize(int samplerate, int block_time_ms)
{
    int i;
    int target;
    int blocksize;

    assert(samplerate > 0);
    blocksize = flac_blocksizes[1];
    target = (samplerate * block_time_ms) / 1000;
    for(i=0; i<16; i++) {
        if(target >= flac_blocksizes[i] && flac_blocksizes[i] > blocksize) {
            blocksize = flac_blocksizes[i];
        }
    }
    return blocksize;
}

static int flac_encode_init(AVCodecContext *avctx)
{
    int freq = avctx->sample_rate;
    int channels = avctx->channels;
    FlacEncodeContext *s = avctx->priv_data;
    int i, level;
    uint8_t *streaminfo;

    s->avctx = avctx;

    if(avctx->sample_fmt != SAMPLE_FMT_S16) {
        return -1;
    }

    if(channels < 1 || channels > FLAC_MAX_CH) {
        return -1;
    }
    s->channels = channels;
    s->ch_code = s->channels-1;

    /* find samplerate in table */
    if(freq < 1)
        return -1;
    for(i=4; i<12; i++) {
        if(freq == flac_samplerates[i]) {
            s->samplerate = flac_samplerates[i];
            s->sr_code[0] = i;
            s->sr_code[1] = 0;
            break;
        }
    }
    /* if not in table, samplerate is non-standard */
    if(i == 12) {
        if(freq % 1000 == 0 && freq < 255000) {
            s->sr_code[0] = 12;
            s->sr_code[1] = freq / 1000;
        } else if(freq % 10 == 0 && freq < 655350) {
            s->sr_code[0] = 14;
            s->sr_code[1] = freq / 10;
        } else if(freq < 65535) {
            s->sr_code[0] = 13;
            s->sr_code[1] = freq;
        } else {
            return -1;
        }
        s->samplerate = freq;
    }

    /* set compression option defaults based on avctx->compression_level */
    if(avctx->compression_level < 0) {
        s->options.compression_level = 5;
    } else {
        s->options.compression_level = avctx->compression_level;
    }
    av_log(avctx, AV_LOG_DEBUG, " compression: %d\n", s->options.compression_level);

    level= s->options.compression_level;
    if(level > 12) {
        av_log(avctx, AV_LOG_ERROR, "invalid compression level: %d\n",
               s->options.compression_level);
        return -1;
    }

    s->options.block_time_ms       = ((int[]){ 27, 27, 27,105,105,105,105,105,105,105,105,105,105})[level];
    s->options.use_lpc             = ((int[]){  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1})[level];
    s->options.min_prediction_order= ((int[]){  2,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1})[level];
    s->options.max_prediction_order= ((int[]){  3,  4,  4,  6,  8,  8,  8,  8, 12, 12, 12, 32, 32})[level];
    s->options.prediction_order_method = ((int[]){ ORDER_METHOD_EST,    ORDER_METHOD_EST,    ORDER_METHOD_EST,
                                                   ORDER_METHOD_EST,    ORDER_METHOD_EST,    ORDER_METHOD_EST,
                                                   ORDER_METHOD_4LEVEL, ORDER_METHOD_LOG,    ORDER_METHOD_4LEVEL,
                                                   ORDER_METHOD_LOG,    ORDER_METHOD_SEARCH, ORDER_METHOD_LOG,
                                                   ORDER_METHOD_SEARCH})[level];
    s->options.min_partition_order = ((int[]){  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0})[level];
    s->options.max_partition_order = ((int[]){  2,  2,  3,  3,  3,  8,  8,  8,  8,  8,  8,  8,  8})[level];

    /* set compression option overrides from AVCodecContext */
    if(avctx->use_lpc >= 0) {
        s->options.use_lpc = clip(avctx->use_lpc, 0, 11);
    }
    if(s->options.use_lpc == 1)
        av_log(avctx, AV_LOG_DEBUG, " use lpc: Levinson-Durbin recursion with Welch window\n");
    else if(s->options.use_lpc > 1)
        av_log(avctx, AV_LOG_DEBUG, " use lpc: Cholesky factorization\n");

    if(avctx->min_prediction_order >= 0) {
        if(s->options.use_lpc) {
            if(avctx->min_prediction_order < MIN_LPC_ORDER ||
                    avctx->min_prediction_order > MAX_LPC_ORDER) {
                av_log(avctx, AV_LOG_ERROR, "invalid min prediction order: %d\n",
                       avctx->min_prediction_order);
                return -1;
            }
        } else {
            if(avctx->min_prediction_order > MAX_FIXED_ORDER) {
                av_log(avctx, AV_LOG_ERROR, "invalid min prediction order: %d\n",
                       avctx->min_prediction_order);
                return -1;
            }
        }
        s->options.min_prediction_order = avctx->min_prediction_order;
    }
    if(avctx->max_prediction_order >= 0) {
        if(s->options.use_lpc) {
            if(avctx->max_prediction_order < MIN_LPC_ORDER ||
                    avctx->max_prediction_order > MAX_LPC_ORDER) {
                av_log(avctx, AV_LOG_ERROR, "invalid max prediction order: %d\n",
                       avctx->max_prediction_order);
                return -1;
            }
        } else {
            if(avctx->max_prediction_order > MAX_FIXED_ORDER) {
                av_log(avctx, AV_LOG_ERROR, "invalid max prediction order: %d\n",
                       avctx->max_prediction_order);
                return -1;
            }
        }
        s->options.max_prediction_order = avctx->max_prediction_order;
    }
    if(s->options.max_prediction_order < s->options.min_prediction_order) {
        av_log(avctx, AV_LOG_ERROR, "invalid prediction orders: min=%d max=%d\n",
               s->options.min_prediction_order, s->options.max_prediction_order);
        return -1;
    }
    av_log(avctx, AV_LOG_DEBUG, " prediction order: %d, %d\n",
           s->options.min_prediction_order, s->options.max_prediction_order);

    if(avctx->prediction_order_method >= 0) {
        if(avctx->prediction_order_method > ORDER_METHOD_LOG) {
            av_log(avctx, AV_LOG_ERROR, "invalid prediction order method: %d\n",
                   avctx->prediction_order_method);
            return -1;
        }
        s->options.prediction_order_method = avctx->prediction_order_method;
    }
    switch(s->options.prediction_order_method) {
        case ORDER_METHOD_EST:    av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "estimate"); break;
        case ORDER_METHOD_2LEVEL: av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "2-level"); break;
        case ORDER_METHOD_4LEVEL: av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "4-level"); break;
        case ORDER_METHOD_8LEVEL: av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "8-level"); break;
        case ORDER_METHOD_SEARCH: av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "full search"); break;
        case ORDER_METHOD_LOG:    av_log(avctx, AV_LOG_DEBUG, " order method: %s\n",
                                         "log search"); break;
    }

    if(avctx->min_partition_order >= 0) {
        if(avctx->min_partition_order > MAX_PARTITION_ORDER) {
            av_log(avctx, AV_LOG_ERROR, "invalid min partition order: %d\n",
                   avctx->min_partition_order);
            return -1;
        }
        s->options.min_partition_order = avctx->min_partition_order;
    }
    if(avctx->max_partition_order >= 0) {
        if(avctx->max_partition_order > MAX_PARTITION_ORDER) {
            av_log(avctx, AV_LOG_ERROR, "invalid max partition order: %d\n",
                   avctx->max_partition_order);
            return -1;
        }
        s->options.max_partition_order = avctx->max_partition_order;
    }
    if(s->options.max_partition_order < s->options.min_partition_order) {
        av_log(avctx, AV_LOG_ERROR, "invalid partition orders: min=%d max=%d\n",
               s->options.min_partition_order, s->options.max_partition_order);
        return -1;
    }
    av_log(avctx, AV_LOG_DEBUG, " partition order: %d, %d\n",
           s->options.min_partition_order, s->options.max_partition_order);

    if(avctx->frame_size > 0) {
        if(avctx->frame_size < FLAC_MIN_BLOCKSIZE ||
                avctx->frame_size > FLAC_MAX_BLOCKSIZE) {
            av_log(avctx, AV_LOG_ERROR, "invalid block size: %d\n",
                   avctx->frame_size);
            return -1;
        }
        s->blocksize = avctx->frame_size;
    } else {
        s->blocksize = select_blocksize(s->samplerate, s->options.block_time_ms);
        avctx->frame_size = s->blocksize;
    }
    av_log(avctx, AV_LOG_DEBUG, " block size: %d\n", s->blocksize);

    /* set LPC precision */
    if(avctx->lpc_coeff_precision > 0) {
        if(avctx->lpc_coeff_precision > MAX_LPC_PRECISION) {
            av_log(avctx, AV_LOG_ERROR, "invalid lpc coeff precision: %d\n",
                   avctx->lpc_coeff_precision);
            return -1;
        }
        s->options.lpc_coeff_precision = avctx->lpc_coeff_precision;
    } else {
        /* select LPC precision based on block size */
        if(     s->blocksize <=   192) s->options.lpc_coeff_precision =  7;
        else if(s->blocksize <=   384) s->options.lpc_coeff_precision =  8;
        else if(s->blocksize <=   576) s->options.lpc_coeff_precision =  9;
        else if(s->blocksize <=  1152) s->options.lpc_coeff_precision = 10;
        else if(s->blocksize <=  2304) s->options.lpc_coeff_precision = 11;
        else if(s->blocksize <=  4608) s->options.lpc_coeff_precision = 12;
        else if(s->blocksize <=  8192) s->options.lpc_coeff_precision = 13;
        else if(s->blocksize <= 16384) s->options.lpc_coeff_precision = 14;
        else                           s->options.lpc_coeff_precision = 15;
    }
    av_log(avctx, AV_LOG_DEBUG, " lpc precision: %d\n",
           s->options.lpc_coeff_precision);

    /* set maximum encoded frame size in verbatim mode */
    if(s->channels == 2) {
        s->max_framesize = 14 + ((s->blocksize * 33 + 7) >> 3);
    } else {
        s->max_framesize = 14 + (s->blocksize * s->channels * 2);
    }

    streaminfo = av_malloc(FLAC_STREAMINFO_SIZE);
    write_streaminfo(s, streaminfo);
    avctx->extradata = streaminfo;
    avctx->extradata_size = FLAC_STREAMINFO_SIZE;

    s->frame_count = 0;

    avctx->coded_frame = avcodec_alloc_frame();
    avctx->coded_frame->key_frame = 1;

    return 0;
}

static void init_frame(FlacEncodeContext *s)
{
    int i, ch;
    FlacFrame *frame;

    frame = &s->frame;

    for(i=0; i<16; i++) {
        if(s->blocksize == flac_blocksizes[i]) {
            frame->blocksize = flac_blocksizes[i];
            frame->bs_code[0] = i;
            frame->bs_code[1] = 0;
            break;
        }
    }
    if(i == 16) {
        frame->blocksize = s->blocksize;
        if(frame->blocksize <= 256) {
            frame->bs_code[0] = 6;
            frame->bs_code[1] = frame->blocksize-1;
        } else {
            frame->bs_code[0] = 7;
            frame->bs_code[1] = frame->blocksize-1;
        }
    }

    for(ch=0; ch<s->channels; ch++) {
        frame->subframes[ch].obits = 16;
    }
}

/**
 * Copy channel-interleaved input samples into separate subframes
 */
static void copy_samples(FlacEncodeContext *s, int16_t *samples)
{
    int i, j, ch;
    FlacFrame *frame;

    frame = &s->frame;
    for(i=0,j=0; i<frame->blocksize; i++) {
        for(ch=0; ch<s->channels; ch++,j++) {
            frame->subframes[ch].samples[i] = samples[j];
        }
    }
}


#define rice_encode_count(sum, n, k) (((n)*((k)+1))+((sum-(n>>1))>>(k)))

static int find_optimal_param(uint32_t sum, int n)
{
    int k, k_opt;
    uint32_t nbits[MAX_RICE_PARAM+1];

    k_opt = 0;
    nbits[0] = UINT32_MAX;
    for(k=0; k<=MAX_RICE_PARAM; k++) {
        nbits[k] = rice_encode_count(sum, n, k);
        if(nbits[k] < nbits[k_opt]) {
            k_opt = k;
        }
    }
    return k_opt;
}

static uint32_t calc_optimal_rice_params(RiceContext *rc, int porder,
                                         uint32_t *sums, int n, int pred_order)
{
    int i;
    int k, cnt, part;
    uint32_t all_bits;

    part = (1 << porder);
    all_bits = 0;

    cnt = (n >> porder) - pred_order;
    for(i=0; i<part; i++) {
        if(i == 1) cnt = (n >> porder);
        k = find_optimal_param(sums[i], cnt);
        rc->params[i] = k;
        all_bits += rice_encode_count(sums[i], cnt, k);
    }
    all_bits += (4 * part);

    rc->porder = porder;

    return all_bits;
}

static void calc_sums(int pmin, int pmax, uint32_t *data, int n, int pred_order,
                      uint32_t sums[][MAX_PARTITIONS])
{
    int i, j;
    int parts;
    uint32_t *res, *res_end;

    /* sums for highest level */
    parts = (1 << pmax);
    res = &data[pred_order];
    res_end = &data[n >> pmax];
    for(i=0; i<parts; i++) {
        sums[pmax][i] = 0;
        while(res < res_end){
            sums[pmax][i] += *(res++);
        }
        res_end+= n >> pmax;
    }
    /* sums for lower levels */
    for(i=pmax-1; i>=pmin; i--) {
        parts = (1 << i);
        for(j=0; j<parts; j++) {
            sums[i][j] = sums[i+1][2*j] + sums[i+1][2*j+1];
        }
    }
}

static uint32_t calc_rice_params(RiceContext *rc, int pmin, int pmax,
                                 int32_t *data, int n, int pred_order)
{
    int i;
    uint32_t bits[MAX_PARTITION_ORDER+1];
    int opt_porder;
    RiceContext tmp_rc;
    uint32_t *udata;
    uint32_t sums[MAX_PARTITION_ORDER+1][MAX_PARTITIONS];

    assert(pmin >= 0 && pmin <= MAX_PARTITION_ORDER);
    assert(pmax >= 0 && pmax <= MAX_PARTITION_ORDER);
    assert(pmin <= pmax);

    udata = av_malloc(n * sizeof(uint32_t));
    for(i=0; i<n; i++) {
        udata[i] = (2*data[i]) ^ (data[i]>>31);
    }

    calc_sums(pmin, pmax, udata, n, pred_order, sums);

    opt_porder = pmin;
    bits[pmin] = UINT32_MAX;
    for(i=pmin; i<=pmax; i++) {
        bits[i] = calc_optimal_rice_params(&tmp_rc, i, sums[i], n, pred_order);
        if(bits[i] <= bits[opt_porder]) {
            opt_porder = i;
            *rc= tmp_rc;
        }
    }

    av_freep(&udata);
    return bits[opt_porder];
}

static int get_max_p_order(int max_porder, int n, int order)
{
    int porder = FFMIN(max_porder, av_log2(n^(n-1)));
    if(order > 0)
        porder = FFMIN(porder, av_log2(n/order));
    return porder;
}

static uint32_t calc_rice_params_fixed(RiceContext *rc, int pmin, int pmax,
                                       int32_t *data, int n, int pred_order,
                                       int bps)
{
    uint32_t bits;
    pmin = get_max_p_order(pmin, n, pred_order);
    pmax = get_max_p_order(pmax, n, pred_order);
    bits = pred_order*bps + 6;
    bits += calc_rice_params(rc, pmin, pmax, data, n, pred_order);
    return bits;
}

static uint32_t calc_rice_params_lpc(RiceContext *rc, int pmin, int pmax,
                                     int32_t *data, int n, int pred_order,
                                     int bps, int precision)
{
    uint32_t bits;
    pmin = get_max_p_order(pmin, n, pred_order);
    pmax = get_max_p_order(pmax, n, pred_order);
    bits = pred_order*bps + 4 + 5 + pred_order*precision + 6;
    bits += calc_rice_params(rc, pmin, pmax, data, n, pred_order);
    return bits;
}

/**
 * Apply Welch window function to audio block
 */
static void apply_welch_window(const int32_t *data, int len, double *w_data)
{
    int i, n2;
    double w;
    double c;

    n2 = (len >> 1);
    c = 2.0 / (len - 1.0);
    for(i=0; i<n2; i++) {
        w = c - i - 1.0;
        w = 1.0 - (w * w);
        w_data[i] = data[i] * w;
        w_data[len-1-i] = data[len-1-i] * w;
    }
}

/**
 * Calculates autocorrelation data from audio samples
 * A Welch window function is applied before calculation.
 */
static void compute_autocorr(const int32_t *data, int len, int lag,
                             double *autoc)
{
    int i, lag_ptr;
    double tmp[len + lag];
    double *data1= tmp + lag;

    apply_welch_window(data, len, data1);

    for(i=0; i<lag; i++){
        autoc[i] = 1.0;
        data1[i-lag]= 0.0;
    }

    for(i=0; i<len; i++){
        for(lag_ptr= i-lag; lag_ptr<=i; lag_ptr++){
            autoc[i-lag_ptr] += data1[i] * data1[lag_ptr];
        }
    }
}

/**
 * Levinson-Durbin recursion.
 * Produces LPC coefficients from autocorrelation data.
 */
static void compute_lpc_coefs(const double *autoc, int max_order,
                              double lpc[][MAX_LPC_ORDER], double *ref)
{
   int i, j, i2;
   double r, err, tmp;
   double lpc_tmp[MAX_LPC_ORDER];

   for(i=0; i<max_order; i++) lpc_tmp[i] = 0;
   err = autoc[0];

   for(i=0; i<max_order; i++) {
      r = -autoc[i+1];
      for(j=0; j<i; j++) {
          r -= lpc_tmp[j] * autoc[i-j];
      }
      r /= err;
      ref[i] = fabs(r);

      err *= 1.0 - (r * r);

      i2 = (i >> 1);
      lpc_tmp[i] = r;
      for(j=0; j<i2; j++) {
         tmp = lpc_tmp[j];
         lpc_tmp[j] += r * lpc_tmp[i-1-j];
         lpc_tmp[i-1-j] += r * tmp;
      }
      if(i & 1) {
          lpc_tmp[j] += lpc_tmp[j] * r;
      }

      for(j=0; j<=i; j++) {
          lpc[i][j] = -lpc_tmp[j];
      }
   }
}

/**
 * Quantize LPC coefficients
 */
static void quantize_lpc_coefs(double *lpc_in, int order, int precision,
                               int32_t *lpc_out, int *shift)
{
    int i;
    double cmax, error;
    int32_t qmax;
    int sh;

    /* define maximum levels */
    qmax = (1 << (precision - 1)) - 1;

    /* find maximum coefficient value */
    cmax = 0.0;
    for(i=0; i<order; i++) {
        cmax= FFMAX(cmax, fabs(lpc_in[i]));
    }

    /* if maximum value quantizes to zero, return all zeros */
    if(cmax * (1 << MAX_LPC_SHIFT) < 1.0) {
        *shift = 0;
        memset(lpc_out, 0, sizeof(int32_t) * order);
        return;
    }

    /* calculate level shift which scales max coeff to available bits */
    sh = MAX_LPC_SHIFT;
    while((cmax * (1 << sh) > qmax) && (sh > 0)) {
        sh--;
    }

    /* since negative shift values are unsupported in decoder, scale down
       coefficients instead */
    if(sh == 0 && cmax > qmax) {
        double scale = ((double)qmax) / cmax;
        for(i=0; i<order; i++) {
            lpc_in[i] *= scale;
        }
    }

    /* output quantized coefficients and level shift */
    error=0;
    for(i=0; i<order; i++) {
        error += lpc_in[i] * (1 << sh);
        lpc_out[i] = clip(lrintf(error), -qmax, qmax);
        error -= lpc_out[i];
    }
    *shift = sh;
}

static int estimate_best_order(double *ref, int max_order)
{
    int i, est;

    est = 1;
    for(i=max_order-1; i>=0; i--) {
        if(ref[i] > 0.10) {
            est = i+1;
            break;
        }
    }
    return est;
}

/**
 * Calculate LPC coefficients for multiple orders
 */
static int lpc_calc_coefs(const int32_t *samples, int blocksize, int max_order,
                          int precision, int32_t coefs[][MAX_LPC_ORDER],
                          int *shift, int use_lpc, int omethod)
{
    double autoc[MAX_LPC_ORDER+1];
    double ref[MAX_LPC_ORDER];
    double lpc[MAX_LPC_ORDER][MAX_LPC_ORDER];
    int i, j, pass;
    int opt_order;

    assert(max_order >= MIN_LPC_ORDER && max_order <= MAX_LPC_ORDER);

    if(use_lpc == 1){
        compute_autocorr(samples, blocksize, max_order+1, autoc);

        compute_lpc_coefs(autoc, max_order, lpc, ref);
    }else{
        LLSModel m[2];
        double var[MAX_LPC_ORDER+1], eval, weight;

        for(pass=0; pass<use_lpc-1; pass++){
            av_init_lls(&m[pass&1], max_order);

            weight=0;
            for(i=max_order; i<blocksize; i++){
                for(j=0; j<=max_order; j++)
                    var[j]= samples[i-j];

                if(pass){
                    eval= av_evaluate_lls(&m[(pass-1)&1], var+1, max_order-1);
                    eval= (512>>pass) + fabs(eval - var[0]);
                    for(j=0; j<=max_order; j++)
                        var[j]/= sqrt(eval);
                    weight += 1/eval;
                }else
                    weight++;

                av_update_lls(&m[pass&1], var, 1.0);
            }
            av_solve_lls(&m[pass&1], 0.001, 0);
        }

        for(i=0; i<max_order; i++){
            for(j=0; j<max_order; j++)
                lpc[i][j]= m[(pass-1)&1].coeff[i][j];
            ref[i]= sqrt(m[(pass-1)&1].variance[i] / weight) * (blocksize - max_order) / 4000;
        }
        for(i=max_order-1; i>0; i--)
            ref[i] = ref[i-1] - ref[i];
    }
    opt_order = max_order;

    if(omethod == ORDER_METHOD_EST) {
        opt_order = estimate_best_order(ref, max_order);
        i = opt_order-1;
        quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
    } else {
        for(i=0; i<max_order; i++) {
            quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
        }
    }

    return opt_order;
}


static void encode_residual_verbatim(int32_t *res, int32_t *smp, int n)
{
    assert(n > 0);
    memcpy(res, smp, n * sizeof(int32_t));
}

static void encode_residual_fixed(int32_t *res, const int32_t *smp, int n,
                                  int order)
{
    int i;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }

    if(order==0){
        for(i=order; i<n; i++)
            res[i]= smp[i];
    }else if(order==1){
        for(i=order; i<n; i++)
            res[i]= smp[i] - smp[i-1];
    }else if(order==2){
        for(i=order; i<n; i++)
            res[i]= smp[i] - 2*smp[i-1] + smp[i-2];
    }else if(order==3){
        for(i=order; i<n; i++)
            res[i]= smp[i] - 3*smp[i-1] + 3*smp[i-2] - smp[i-3];
    }else{
        for(i=order; i<n; i++)
            res[i]= smp[i] - 4*smp[i-1] + 6*smp[i-2] - 4*smp[i-3] + smp[i-4];
    }
}

static void encode_residual_lpc(int32_t *res, const int32_t *smp, int n,
                                int order, const int32_t *coefs, int shift)
{
    int i, j;
    int32_t pred;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }
    for(i=order; i<n; i++) {
        pred = 0;
        for(j=0; j<order; j++) {
            pred += coefs[j] * smp[i-j-1];
        }
        res[i] = smp[i] - (pred >> shift);
    }
}

static int encode_residual(FlacEncodeContext *ctx, int ch)
{
    int i, n;
    int min_order, max_order, opt_order, precision, omethod;
    int min_porder, max_porder;
    FlacFrame *frame;
    FlacSubframe *sub;
    int32_t coefs[MAX_LPC_ORDER][MAX_LPC_ORDER];
    int shift[MAX_LPC_ORDER];
    int32_t *res, *smp;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    res = sub->residual;
    smp = sub->samples;
    n = frame->blocksize;

    /* CONSTANT */
    for(i=1; i<n; i++) {
        if(smp[i] != smp[0]) break;
    }
    if(i == n) {
        sub->type = sub->type_code = FLAC_SUBFRAME_CONSTANT;
        res[0] = smp[0];
        return sub->obits;
    }

    /* VERBATIM */
    if(n < 5) {
        sub->type = sub->type_code = FLAC_SUBFRAME_VERBATIM;
        encode_residual_verbatim(res, smp, n);
        return sub->obits * n;
    }

    min_order = ctx->options.min_prediction_order;
    max_order = ctx->options.max_prediction_order;
    min_porder = ctx->options.min_partition_order;
    max_porder = ctx->options.max_partition_order;
    precision = ctx->options.lpc_coeff_precision;
    omethod = ctx->options.prediction_order_method;

    /* FIXED */
    if(!ctx->options.use_lpc || max_order == 0 || (n <= max_order)) {
        uint32_t bits[MAX_FIXED_ORDER+1];
        if(max_order > MAX_FIXED_ORDER) max_order = MAX_FIXED_ORDER;
        opt_order = 0;
        bits[0] = UINT32_MAX;
        for(i=min_order; i<=max_order; i++) {
            encode_residual_fixed(res, smp, n, i);
            bits[i] = calc_rice_params_fixed(&sub->rc, min_porder, max_porder, res,
                                             n, i, sub->obits);
            if(bits[i] < bits[opt_order]) {
                opt_order = i;
            }
        }
        sub->order = opt_order;
        sub->type = FLAC_SUBFRAME_FIXED;
        sub->type_code = sub->type | sub->order;
        if(sub->order != max_order) {
            encode_residual_fixed(res, smp, n, sub->order);
            return calc_rice_params_fixed(&sub->rc, min_porder, max_porder, res, n,
                                          sub->order, sub->obits);
        }
        return bits[sub->order];
    }

    /* LPC */
    opt_order = lpc_calc_coefs(smp, n, max_order, precision, coefs, shift, ctx->options.use_lpc, omethod);

    if(omethod == ORDER_METHOD_2LEVEL ||
       omethod == ORDER_METHOD_4LEVEL ||
       omethod == ORDER_METHOD_8LEVEL) {
        int levels = 1 << omethod;
        uint32_t bits[levels];
        int order;
        int opt_index = levels-1;
        opt_order = max_order-1;
        bits[opt_index] = UINT32_MAX;
        for(i=levels-1; i>=0; i--) {
            order = min_order + (((max_order-min_order+1) * (i+1)) / levels)-1;
            if(order < 0) order = 0;
            encode_residual_lpc(res, smp, n, order+1, coefs[order], shift[order]);
            bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                           res, n, order+1, sub->obits, precision);
            if(bits[i] < bits[opt_index]) {
                opt_index = i;
                opt_order = order;
            }
        }
        opt_order++;
    } else if(omethod == ORDER_METHOD_SEARCH) {
        // brute-force optimal order search
        uint32_t bits[MAX_LPC_ORDER];
        opt_order = 0;
        bits[0] = UINT32_MAX;
        for(i=min_order-1; i<max_order; i++) {
            encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i]);
            bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                           res, n, i+1, sub->obits, precision);
            if(bits[i] < bits[opt_order]) {
                opt_order = i;
            }
        }
        opt_order++;
    } else if(omethod == ORDER_METHOD_LOG) {
        uint32_t bits[MAX_LPC_ORDER];
        int step;

        opt_order= min_order - 1 + (max_order-min_order)/3;
        memset(bits, -1, sizeof(bits));

        for(step=16 ;step; step>>=1){
            int last= opt_order;
            for(i=last-step; i<=last+step; i+= step){
                if(i<min_order-1 || i>=max_order || bits[i] < UINT32_MAX)
                    continue;
                encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i]);
                bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                            res, n, i+1, sub->obits, precision);
                if(bits[i] < bits[opt_order])
                    opt_order= i;
            }
        }
        opt_order++;
    }

    sub->order = opt_order;
    sub->type = FLAC_SUBFRAME_LPC;
    sub->type_code = sub->type | (sub->order-1);
    sub->shift = shift[sub->order-1];
    for(i=0; i<sub->order; i++) {
        sub->coefs[i] = coefs[sub->order-1][i];
    }
    encode_residual_lpc(res, smp, n, sub->order, sub->coefs, sub->shift);
    return calc_rice_params_lpc(&sub->rc, min_porder, max_porder, res, n, sub->order,
                                sub->obits, precision);
}

static int encode_residual_v(FlacEncodeContext *ctx, int ch)
{
    int i, n;
    FlacFrame *frame;
    FlacSubframe *sub;
    int32_t *res, *smp;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    res = sub->residual;
    smp = sub->samples;
    n = frame->blocksize;

    /* CONSTANT */
    for(i=1; i<n; i++) {
        if(smp[i] != smp[0]) break;
    }
    if(i == n) {
        sub->type = sub->type_code = FLAC_SUBFRAME_CONSTANT;
        res[0] = smp[0];
        return sub->obits;
    }

    /* VERBATIM */
    sub->type = sub->type_code = FLAC_SUBFRAME_VERBATIM;
    encode_residual_verbatim(res, smp, n);
    return sub->obits * n;
}

static int estimate_stereo_mode(int32_t *left_ch, int32_t *right_ch, int n)
{
    int i, best;
    int32_t lt, rt;
    uint64_t sum[4];
    uint64_t score[4];
    int k;

    /* calculate sum of 2nd order residual for each channel */
    sum[0] = sum[1] = sum[2] = sum[3] = 0;
    for(i=2; i<n; i++) {
        lt = left_ch[i] - 2*left_ch[i-1] + left_ch[i-2];
        rt = right_ch[i] - 2*right_ch[i-1] + right_ch[i-2];
        sum[2] += ABS((lt + rt) >> 1);
        sum[3] += ABS(lt - rt);
        sum[0] += ABS(lt);
        sum[1] += ABS(rt);
    }
    /* estimate bit counts */
    for(i=0; i<4; i++) {
        k = find_optimal_param(2*sum[i], n);
        sum[i] = rice_encode_count(2*sum[i], n, k);
    }

    /* calculate score for each mode */
    score[0] = sum[0] + sum[1];
    score[1] = sum[0] + sum[3];
    score[2] = sum[1] + sum[3];
    score[3] = sum[2] + sum[3];

    /* return mode with lowest score */
    best = 0;
    for(i=1; i<4; i++) {
        if(score[i] < score[best]) {
            best = i;
        }
    }
    if(best == 0) {
        return FLAC_CHMODE_LEFT_RIGHT;
    } else if(best == 1) {
        return FLAC_CHMODE_LEFT_SIDE;
    } else if(best == 2) {
        return FLAC_CHMODE_RIGHT_SIDE;
    } else {
        return FLAC_CHMODE_MID_SIDE;
    }
}

/**
 * Perform stereo channel decorrelation
 */
static void channel_decorrelation(FlacEncodeContext *ctx)
{
    FlacFrame *frame;
    int32_t *left, *right;
    int i, n;

    frame = &ctx->frame;
    n = frame->blocksize;
    left  = frame->subframes[0].samples;
    right = frame->subframes[1].samples;

    if(ctx->channels != 2) {
        frame->ch_mode = FLAC_CHMODE_NOT_STEREO;
        return;
    }

    frame->ch_mode = estimate_stereo_mode(left, right, n);

    /* perform decorrelation and adjust bits-per-sample */
    if(frame->ch_mode == FLAC_CHMODE_LEFT_RIGHT) {
        return;
    }
    if(frame->ch_mode == FLAC_CHMODE_MID_SIDE) {
        int32_t tmp;
        for(i=0; i<n; i++) {
            tmp = left[i];
            left[i] = (tmp + right[i]) >> 1;
            right[i] = tmp - right[i];
        }
        frame->subframes[1].obits++;
    } else if(frame->ch_mode == FLAC_CHMODE_LEFT_SIDE) {
        for(i=0; i<n; i++) {
            right[i] = left[i] - right[i];
        }
        frame->subframes[1].obits++;
    } else {
        for(i=0; i<n; i++) {
            left[i] -= right[i];
        }
        frame->subframes[0].obits++;
    }
}

static void put_sbits(PutBitContext *pb, int bits, int32_t val)
{
    assert(bits >= 0 && bits <= 31);

    put_bits(pb, bits, val & ((1<<bits)-1));
}

static void write_utf8(PutBitContext *pb, uint32_t val)
{
    int bytes, shift;

    if(val < 0x80){
        put_bits(pb, 8, val);
        return;
    }

    bytes= (av_log2(val)+4) / 5;
    shift = (bytes - 1) * 6;
    put_bits(pb, 8, (256 - (256>>bytes)) | (val >> shift));
    while(shift >= 6){
        shift -= 6;
        put_bits(pb, 8, 0x80 | ((val >> shift) & 0x3F));
    }
}

static void output_frame_header(FlacEncodeContext *s)
{
    FlacFrame *frame;
    int crc;

    frame = &s->frame;

    put_bits(&s->pb, 16, 0xFFF8);
    put_bits(&s->pb, 4, frame->bs_code[0]);
    put_bits(&s->pb, 4, s->sr_code[0]);
    if(frame->ch_mode == FLAC_CHMODE_NOT_STEREO) {
        put_bits(&s->pb, 4, s->ch_code);
    } else {
        put_bits(&s->pb, 4, frame->ch_mode);
    }
    put_bits(&s->pb, 3, 4); /* bits-per-sample code */
    put_bits(&s->pb, 1, 0);
    write_utf8(&s->pb, s->frame_count);
    if(frame->bs_code[0] == 6) {
        put_bits(&s->pb, 8, frame->bs_code[1]);
    } else if(frame->bs_code[0] == 7) {
        put_bits(&s->pb, 16, frame->bs_code[1]);
    }
    if(s->sr_code[0] == 12) {
        put_bits(&s->pb, 8, s->sr_code[1]);
    } else if(s->sr_code[0] > 12) {
        put_bits(&s->pb, 16, s->sr_code[1]);
    }
    flush_put_bits(&s->pb);
    crc = av_crc(av_crc07, 0, s->pb.buf, put_bits_count(&s->pb)>>3);
    put_bits(&s->pb, 8, crc);
}

static void output_subframe_constant(FlacEncodeContext *s, int ch)
{
    FlacSubframe *sub;
    int32_t res;

    sub = &s->frame.subframes[ch];
    res = sub->residual[0];
    put_sbits(&s->pb, sub->obits, res);
}

static void output_subframe_verbatim(FlacEncodeContext *s, int ch)
{
    int i;
    FlacFrame *frame;
    FlacSubframe *sub;
    int32_t res;

    frame = &s->frame;
    sub = &frame->subframes[ch];

    for(i=0; i<frame->blocksize; i++) {
        res = sub->residual[i];
        put_sbits(&s->pb, sub->obits, res);
    }
}

static void output_residual(FlacEncodeContext *ctx, int ch)
{
    int i, j, p, n, parts;
    int k, porder, psize, res_cnt;
    FlacFrame *frame;
    FlacSubframe *sub;
    int32_t *res;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    res = sub->residual;
    n = frame->blocksize;

    /* rice-encoded block */
    put_bits(&ctx->pb, 2, 0);

    /* partition order */
    porder = sub->rc.porder;
    psize = n >> porder;
    parts = (1 << porder);
    put_bits(&ctx->pb, 4, porder);
    res_cnt = psize - sub->order;

    /* residual */
    j = sub->order;
    for(p=0; p<parts; p++) {
        k = sub->rc.params[p];
        put_bits(&ctx->pb, 4, k);
        if(p == 1) res_cnt = psize;
        for(i=0; i<res_cnt && j<n; i++, j++) {
            set_sr_golomb_flac(&ctx->pb, res[j], k, INT32_MAX, 0);
        }
    }
}

static void output_subframe_fixed(FlacEncodeContext *ctx, int ch)
{
    int i;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    /* warm-up samples */
    for(i=0; i<sub->order; i++) {
        put_sbits(&ctx->pb, sub->obits, sub->residual[i]);
    }

    /* residual */
    output_residual(ctx, ch);
}

static void output_subframe_lpc(FlacEncodeContext *ctx, int ch)
{
    int i, cbits;
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    /* warm-up samples */
    for(i=0; i<sub->order; i++) {
        put_sbits(&ctx->pb, sub->obits, sub->residual[i]);
    }

    /* LPC coefficients */
    cbits = ctx->options.lpc_coeff_precision;
    put_bits(&ctx->pb, 4, cbits-1);
    put_sbits(&ctx->pb, 5, sub->shift);
    for(i=0; i<sub->order; i++) {
        put_sbits(&ctx->pb, cbits, sub->coefs[i]);
    }

    /* residual */
    output_residual(ctx, ch);
}

static void output_subframes(FlacEncodeContext *s)
{
    FlacFrame *frame;
    FlacSubframe *sub;
    int ch;

    frame = &s->frame;

    for(ch=0; ch<s->channels; ch++) {
        sub = &frame->subframes[ch];

        /* subframe header */
        put_bits(&s->pb, 1, 0);
        put_bits(&s->pb, 6, sub->type_code);
        put_bits(&s->pb, 1, 0); /* no wasted bits */

        /* subframe */
        if(sub->type == FLAC_SUBFRAME_CONSTANT) {
            output_subframe_constant(s, ch);
        } else if(sub->type == FLAC_SUBFRAME_VERBATIM) {
            output_subframe_verbatim(s, ch);
        } else if(sub->type == FLAC_SUBFRAME_FIXED) {
            output_subframe_fixed(s, ch);
        } else if(sub->type == FLAC_SUBFRAME_LPC) {
            output_subframe_lpc(s, ch);
        }
    }
}

static void output_frame_footer(FlacEncodeContext *s)
{
    int crc;
    flush_put_bits(&s->pb);
    crc = bswap_16(av_crc(av_crc8005, 0, s->pb.buf, put_bits_count(&s->pb)>>3));
    put_bits(&s->pb, 16, crc);
    flush_put_bits(&s->pb);
}

static int flac_encode_frame(AVCodecContext *avctx, uint8_t *frame,
                             int buf_size, void *data)
{
    int ch;
    FlacEncodeContext *s;
    int16_t *samples = data;
    int out_bytes;

    s = avctx->priv_data;

    s->blocksize = avctx->frame_size;
    init_frame(s);

    copy_samples(s, samples);

    channel_decorrelation(s);

    for(ch=0; ch<s->channels; ch++) {
        encode_residual(s, ch);
    }
    init_put_bits(&s->pb, frame, buf_size);
    output_frame_header(s);
    output_subframes(s);
    output_frame_footer(s);
    out_bytes = put_bits_count(&s->pb) >> 3;

    if(out_bytes > s->max_framesize || out_bytes >= buf_size) {
        /* frame too large. use verbatim mode */
        for(ch=0; ch<s->channels; ch++) {
            encode_residual_v(s, ch);
        }
        init_put_bits(&s->pb, frame, buf_size);
        output_frame_header(s);
        output_subframes(s);
        output_frame_footer(s);
        out_bytes = put_bits_count(&s->pb) >> 3;

        if(out_bytes > s->max_framesize || out_bytes >= buf_size) {
            /* still too large. must be an error. */
            av_log(avctx, AV_LOG_ERROR, "error encoding frame\n");
            return -1;
        }
    }

    s->frame_count++;
    return out_bytes;
}

static int flac_encode_close(AVCodecContext *avctx)
{
    av_freep(&avctx->extradata);
    avctx->extradata_size = 0;
    av_freep(&avctx->coded_frame);
    return 0;
}

AVCodec flac_encoder = {
    "flac",
    CODEC_TYPE_AUDIO,
    CODEC_ID_FLAC,
    sizeof(FlacEncodeContext),
    flac_encode_init,
    flac_encode_frame,
    flac_encode_close,
    NULL,
    .capabilities = CODEC_CAP_SMALL_LAST_FRAME,
};
