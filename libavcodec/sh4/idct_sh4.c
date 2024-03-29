/*
 * idct for sh4
 *
 * Copyright (c) 2001-2003 BERO <bero@geocities.co.jp>
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

#include "../dsputil.h"
#define c1      1.38703984532214752434  /* sqrt(2)*cos(1*pi/16) */
#define c2      1.30656296487637657577  /* sqrt(2)*cos(2*pi/16) */
#define c3      1.17587560241935884520  /* sqrt(2)*cos(3*pi/16) */
#define c4      1.00000000000000000000  /* sqrt(2)*cos(4*pi/16) */
#define c5      0.78569495838710234903  /* sqrt(2)*cos(5*pi/16) */
#define c6      0.54119610014619712324  /* sqrt(2)*cos(6*pi/16) */
#define c7      0.27589937928294311353  /* sqrt(2)*cos(7*pi/16) */

static const float even_table[] __attribute__ ((aligned(8))) = {
        c4, c4, c4, c4,
        c2, c6,-c6,-c2,
        c4,-c4,-c4, c4,
        c6,-c2, c2,-c6
};

static const float odd_table[] __attribute__ ((aligned(8))) = {
        c1, c3, c5, c7,
        c3,-c7,-c1,-c5,
        c5,-c1, c7, c3,
        c7,-c5, c3,-c1
};

#undef  c1
#undef  c2
#undef  c3
#undef  c4
#undef  c5
#undef  c6
#undef  c7

#if defined(__SH4_SINGLE__) || defined(__SH4_SINGLE_ONLY__)

#define         load_matrix(table) \
        __asm__ volatile( \
        "       fschg\n" \
        "       fmov   @%0+,xd0\n" \
        "       fmov   @%0+,xd2\n" \
        "       fmov   @%0+,xd4\n" \
        "       fmov   @%0+,xd6\n" \
        "       fmov   @%0+,xd8\n" \
        "       fmov   @%0+,xd10\n" \
        "       fmov   @%0+,xd12\n" \
        "       fmov   @%0+,xd14\n" \
        "       fschg\n" \
        :\
        : "r"(table)\
        : "0" \
        )

#define         ftrv() \
                __asm__ volatile("ftrv xmtrx,fv0" \
                : "=f"(fr0),"=f"(fr1),"=f"(fr2),"=f"(fr3) \
                :  "0"(fr0), "1"(fr1), "2"(fr2), "3"(fr3) );

#define         DEFREG        \
        register float fr0 __asm__("fr0"); \
        register float fr1 __asm__("fr1"); \
        register float fr2 __asm__("fr2"); \
        register float fr3 __asm__("fr3")

#else

/* generic C code for check */

static void ftrv_(const float xf[],float fv[])
{
        float f0,f1,f2,f3;
        f0 = fv[0];
        f1 = fv[1];
        f2 = fv[2];
        f3 = fv[3];
        fv[0] = xf[0]*f0 + xf[4]*f1 + xf[ 8]*f2 + xf[12]*f3;
        fv[1] = xf[1]*f0 + xf[5]*f1 + xf[ 9]*f2 + xf[13]*f3;
        fv[2] = xf[2]*f0 + xf[6]*f1 + xf[10]*f2 + xf[14]*f3;
        fv[3] = xf[3]*f0 + xf[7]*f1 + xf[11]*f2 + xf[15]*f3;
}

static void load_matrix_(float xf[],const float table[])
{
        int i;
        for(i=0;i<16;i++) xf[i]=table[i];
}

#define         ftrv()                  ftrv_(xf,fv)
#define         load_matrix(table)      load_matrix_(xf,table)

#define         DEFREG \
        float fv[4],xf[16]

#define         fr0     fv[0]
#define         fr1     fv[1]
#define         fr2     fv[2]
#define         fr3     fv[3]

#endif

#if 1
#define         DESCALE(x,n)    (x)*(1.0f/(1<<(n)))
#else
#define         DESCALE(x,n)    (((int)(x)+(1<<(n-1)))>>(n))
#endif

/* this code work worse on gcc cvs. 3.2.3 work fine */


#if 1
//optimized

void idct_sh4(DCTELEM *block)
{
        DEFREG;

        int i;
        float        tblock[8*8],*fblock;
        int ofs1,ofs2,ofs3;

#if defined(__SH4__)
#error  "FIXME!! change to single float"
#endif

        /* row */

        /* even part */
        load_matrix(even_table);

        fblock = tblock+4;
        i = 8;
        do {
                fr0 = block[0];
                fr1 = block[2];
                fr2 = block[4];
                fr3 = block[6];
                block+=8;
                ftrv();
                *--fblock = fr3;
                *--fblock = fr2;
                *--fblock = fr1;
                *--fblock = fr0;
                fblock+=8+4;
        } while(--i);
        block-=8*8;
        fblock-=8*8+4;

        load_matrix(odd_table);

        i = 8;

//        ofs1 = sizeof(float)*1;
//        ofs2 = sizeof(float)*2;
//        ofs3 = sizeof(float)*3;

        do {
                float t0,t1,t2,t3;
                fr0 = block[1];
                fr1 = block[3];
                fr2 = block[5];
                fr3 = block[7];
                block+=8;
                ftrv();
                t0 = *fblock++;
                t1 = *fblock++;
                t2 = *fblock++;
                t3 = *fblock++;
                fblock+=4;
                *--fblock = t0 - fr0;
                *--fblock = t1 - fr1;
                *--fblock = t2 - fr2;
                *--fblock = t3 - fr3;
                *--fblock = t3 + fr3;
                *--fblock = t2 + fr2;
                *--fblock = t1 + fr1;
                *--fblock = t0 + fr0;
                fblock+=8;
        } while(--i);
        block-=8*8;
        fblock-=8*8;

        /* col */

        /* even part */
        load_matrix(even_table);

        ofs1 = sizeof(float)*2*8;
        ofs2 = sizeof(float)*4*8;
        ofs3 = sizeof(float)*6*8;

        i = 8;

#define        OA(fblock,ofs)   *(float*)((char*)fblock + ofs)

        do {
                fr0 = OA(fblock,   0);
                fr1 = OA(fblock,ofs1);
                fr2 = OA(fblock,ofs2);
                fr3 = OA(fblock,ofs3);
                ftrv();
                OA(fblock,0   ) = fr0;
                OA(fblock,ofs1) = fr1;
                OA(fblock,ofs2) = fr2;
                OA(fblock,ofs3) = fr3;
                fblock++;
        } while(--i);
        fblock-=8;

        load_matrix(odd_table);

        i=8;
        do {
                float t0,t1,t2,t3;
                t0 = OA(fblock,   0); /* [8*0] */
                t1 = OA(fblock,ofs1); /* [8*2] */
                t2 = OA(fblock,ofs2); /* [8*4] */
                t3 = OA(fblock,ofs3); /* [8*6] */
                fblock+=8;
                fr0 = OA(fblock,   0); /* [8*1] */
                fr1 = OA(fblock,ofs1); /* [8*3] */
                fr2 = OA(fblock,ofs2); /* [8*5] */
                fr3 = OA(fblock,ofs3); /* [8*7] */
                fblock+=-8+1;
                ftrv();
                block[8*0] = DESCALE(t0 + fr0,3);
                block[8*7] = DESCALE(t0 - fr0,3);
                block[8*1] = DESCALE(t1 + fr1,3);
                block[8*6] = DESCALE(t1 - fr1,3);
                block[8*2] = DESCALE(t2 + fr2,3);
                block[8*5] = DESCALE(t2 - fr2,3);
                block[8*3] = DESCALE(t3 + fr3,3);
                block[8*4] = DESCALE(t3 - fr3,3);
                block++;
        } while(--i);

#if defined(__SH4__)
#error  "FIXME!! change to double"
#endif
}
#else
void idct_sh4(DCTELEM *block)
{
        DEFREG;

        int i;
        float   tblock[8*8],*fblock;

        /* row */

        /* even part */
        load_matrix(even_table);

        fblock = tblock;
        i = 8;
        do {
                fr0 = block[0];
                fr1 = block[2];
                fr2 = block[4];
                fr3 = block[6];
                block+=8;
                ftrv();
                fblock[0] = fr0;
                fblock[2] = fr1;
                fblock[4] = fr2;
                fblock[6] = fr3;
                fblock+=8;
        } while(--i);
        block-=8*8;
        fblock-=8*8;

        load_matrix(odd_table);

        i = 8;

        do {
                float t0,t1,t2,t3;
                fr0 = block[1];
                fr1 = block[3];
                fr2 = block[5];
                fr3 = block[7];
                block+=8;
                ftrv();
                t0 = fblock[0];
                t1 = fblock[2];
                t2 = fblock[4];
                t3 = fblock[6];
                fblock[0] = t0 + fr0;
                fblock[7] = t0 - fr0;
                fblock[1] = t1 + fr1;
                fblock[6] = t1 - fr1;
                fblock[2] = t2 + fr2;
                fblock[5] = t2 - fr2;
                fblock[3] = t3 + fr3;
                fblock[4] = t3 - fr3;
                fblock+=8;
        } while(--i);
        block-=8*8;
        fblock-=8*8;

        /* col */

        /* even part */
        load_matrix(even_table);

        i = 8;

        do {
                fr0 = fblock[8*0];
                fr1 = fblock[8*2];
                fr2 = fblock[8*4];
                fr3 = fblock[8*6];
                ftrv();
                fblock[8*0] = fr0;
                fblock[8*2] = fr1;
                fblock[8*4] = fr2;
                fblock[8*6] = fr3;
                fblock++;
        } while(--i);
        fblock-=8;

        load_matrix(odd_table);

        i=8;
        do {
                float t0,t1,t2,t3;
                fr0 = fblock[8*1];
                fr1 = fblock[8*3];
                fr2 = fblock[8*5];
                fr3 = fblock[8*7];
                ftrv();
                t0 = fblock[8*0];
                t1 = fblock[8*2];
                t2 = fblock[8*4];
                t3 = fblock[8*6];
                fblock++;
                block[8*0] = DESCALE(t0 + fr0,3);
                block[8*7] = DESCALE(t0 - fr0,3);
                block[8*1] = DESCALE(t1 + fr1,3);
                block[8*6] = DESCALE(t1 - fr1,3);
                block[8*2] = DESCALE(t2 + fr2,3);
                block[8*5] = DESCALE(t2 - fr2,3);
                block[8*3] = DESCALE(t3 + fr3,3);
                block[8*4] = DESCALE(t3 - fr3,3);
                block++;
        } while(--i);
}
#endif
