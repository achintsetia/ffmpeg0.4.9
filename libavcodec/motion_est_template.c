/*
 * Motion estimation
 * Copyright (c) 2002-2004 Michael Niedermayer
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
 *
 */

/**
 * @file motion_est_template.c
 * Motion estimation template.
 */

//lets hope gcc will remove the unused vars ...(gcc 3.2.2 seems to do it ...)
#define LOAD_COMMON\
    uint32_t attribute_unused * const score_map= c->score_map;\
    const int attribute_unused xmin= c->xmin;\
    const int attribute_unused ymin= c->ymin;\
    const int attribute_unused xmax= c->xmax;\
    const int attribute_unused ymax= c->ymax;\
    uint8_t *mv_penalty= c->current_mv_penalty;\
    const int pred_x= c->pred_x;\
    const int pred_y= c->pred_y;\

#define CHECK_HALF_MV(dx, dy, x, y)\
{\
    const int hx= 2*(x)+(dx);\
    const int hy= 2*(y)+(dy);\
    d= cmp(s, x, y, dx, dy, size, h, ref_index, src_index, cmp_sub, chroma_cmp_sub, flags);\
    d += (mv_penalty[hx - pred_x] + mv_penalty[hy - pred_y])*penalty_factor;\
    COPY3_IF_LT(dmin, d, bx, hx, by, hy)\
}

#if 0
static int hpel_motion_search)(MpegEncContext * s,
                                  int *mx_ptr, int *my_ptr, int dmin,
                                  uint8_t *ref_data[3],
                                  int size)
{
    const int xx = 16 * s->mb_x + 8*(n&1);
    const int yy = 16 * s->mb_y + 8*(n>>1);
    const int mx = *mx_ptr;
    const int my = *my_ptr;
    const int penalty_factor= c->sub_penalty_factor;

    LOAD_COMMON

 //   INIT;
 //FIXME factorize
    me_cmp_func cmp, chroma_cmp, cmp_sub, chroma_cmp_sub;

    if(s->no_rounding /*FIXME b_type*/){
        hpel_put= &s->dsp.put_no_rnd_pixels_tab[size];
        chroma_hpel_put= &s->dsp.put_no_rnd_pixels_tab[size+1];
    }else{
        hpel_put=& s->dsp.put_pixels_tab[size];
        chroma_hpel_put= &s->dsp.put_pixels_tab[size+1];
    }
    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];
    cmp_sub= s->dsp.me_sub_cmp[size];
    chroma_cmp_sub= s->dsp.me_sub_cmp[size+1];

    if(c->skip){ //FIXME somehow move up (benchmark)
        *mx_ptr = 0;
        *my_ptr = 0;
        return dmin;
    }

    if(c->avctx->me_cmp != c->avctx->me_sub_cmp){
        CMP_HPEL(dmin, 0, 0, mx, my, size);
        if(mx || my)
            dmin += (mv_penalty[2*mx - pred_x] + mv_penalty[2*my - pred_y])*penalty_factor;
    }

    if (mx > xmin && mx < xmax &&
        my > ymin && my < ymax) {
        int bx=2*mx, by=2*my;
        int d= dmin;

        CHECK_HALF_MV(1, 1, mx-1, my-1)
        CHECK_HALF_MV(0, 1, mx  , my-1)
        CHECK_HALF_MV(1, 1, mx  , my-1)
        CHECK_HALF_MV(1, 0, mx-1, my  )
        CHECK_HALF_MV(1, 0, mx  , my  )
        CHECK_HALF_MV(1, 1, mx-1, my  )
        CHECK_HALF_MV(0, 1, mx  , my  )
        CHECK_HALF_MV(1, 1, mx  , my  )

        assert(bx >= xmin*2 || bx <= xmax*2 || by >= ymin*2 || by <= ymax*2);

        *mx_ptr = bx;
        *my_ptr = by;
    }else{
        *mx_ptr =2*mx;
        *my_ptr =2*my;
    }

    return dmin;
}

#else
static int hpel_motion_search(MpegEncContext * s,
                                  int *mx_ptr, int *my_ptr, int dmin,
                                  int src_index, int ref_index,
                                  int size, int h)
{
    MotionEstContext * const c= &s->me;
    const int mx = *mx_ptr;
    const int my = *my_ptr;
    const int penalty_factor= c->sub_penalty_factor;
    me_cmp_func cmp_sub, chroma_cmp_sub;
    int bx=2*mx, by=2*my;

    LOAD_COMMON
    int flags= c->sub_flags;

 //FIXME factorize

    cmp_sub= s->dsp.me_sub_cmp[size];
    chroma_cmp_sub= s->dsp.me_sub_cmp[size+1];

    if(c->skip){ //FIXME move out of hpel?
        *mx_ptr = 0;
        *my_ptr = 0;
        return dmin;
    }

    if(c->avctx->me_cmp != c->avctx->me_sub_cmp){
        dmin= cmp(s, mx, my, 0, 0, size, h, ref_index, src_index, cmp_sub, chroma_cmp_sub, flags);
        if(mx || my || size>0)
            dmin += (mv_penalty[2*mx - pred_x] + mv_penalty[2*my - pred_y])*penalty_factor;
    }

    if (mx > xmin && mx < xmax &&
        my > ymin && my < ymax) {
        int d= dmin;
        const int index= (my<<ME_MAP_SHIFT) + mx;
        const int t= score_map[(index-(1<<ME_MAP_SHIFT))&(ME_MAP_SIZE-1)]
                     + (mv_penalty[bx   - pred_x] + mv_penalty[by-2 - pred_y])*c->penalty_factor;
        const int l= score_map[(index- 1               )&(ME_MAP_SIZE-1)]
                     + (mv_penalty[bx-2 - pred_x] + mv_penalty[by   - pred_y])*c->penalty_factor;
        const int r= score_map[(index+ 1               )&(ME_MAP_SIZE-1)]
                     + (mv_penalty[bx+2 - pred_x] + mv_penalty[by   - pred_y])*c->penalty_factor;
        const int b= score_map[(index+(1<<ME_MAP_SHIFT))&(ME_MAP_SIZE-1)]
                     + (mv_penalty[bx   - pred_x] + mv_penalty[by+2 - pred_y])*c->penalty_factor;

#if 1
        int key;
        int map_generation= c->map_generation;
#ifndef NDEBUG
        uint32_t *map= c->map;
#endif
        key= ((my-1)<<ME_MAP_MV_BITS) + (mx) + map_generation;
        assert(map[(index-(1<<ME_MAP_SHIFT))&(ME_MAP_SIZE-1)] == key);
        key= ((my+1)<<ME_MAP_MV_BITS) + (mx) + map_generation;
        assert(map[(index+(1<<ME_MAP_SHIFT))&(ME_MAP_SIZE-1)] == key);
        key= ((my)<<ME_MAP_MV_BITS) + (mx+1) + map_generation;
        assert(map[(index+1)&(ME_MAP_SIZE-1)] == key);
        key= ((my)<<ME_MAP_MV_BITS) + (mx-1) + map_generation;
        assert(map[(index-1)&(ME_MAP_SIZE-1)] == key);
#endif
        if(t<=b){
            CHECK_HALF_MV(0, 1, mx  ,my-1)
            if(l<=r){
                CHECK_HALF_MV(1, 1, mx-1, my-1)
                if(t+r<=b+l){
                    CHECK_HALF_MV(1, 1, mx  , my-1)
                }else{
                    CHECK_HALF_MV(1, 1, mx-1, my  )
                }
                CHECK_HALF_MV(1, 0, mx-1, my  )
            }else{
                CHECK_HALF_MV(1, 1, mx  , my-1)
                if(t+l<=b+r){
                    CHECK_HALF_MV(1, 1, mx-1, my-1)
                }else{
                    CHECK_HALF_MV(1, 1, mx  , my  )
                }
                CHECK_HALF_MV(1, 0, mx  , my  )
            }
        }else{
            if(l<=r){
                if(t+l<=b+r){
                    CHECK_HALF_MV(1, 1, mx-1, my-1)
                }else{
                    CHECK_HALF_MV(1, 1, mx  , my  )
                }
                CHECK_HALF_MV(1, 0, mx-1, my)
                CHECK_HALF_MV(1, 1, mx-1, my)
            }else{
                if(t+r<=b+l){
                    CHECK_HALF_MV(1, 1, mx  , my-1)
                }else{
                    CHECK_HALF_MV(1, 1, mx-1, my)
                }
                CHECK_HALF_MV(1, 0, mx  , my)
                CHECK_HALF_MV(1, 1, mx  , my)
            }
            CHECK_HALF_MV(0, 1, mx  , my)
        }
        assert(bx >= xmin*2 && bx <= xmax*2 && by >= ymin*2 && by <= ymax*2);
    }

    *mx_ptr = bx;
    *my_ptr = by;

    return dmin;
}
#endif

static int no_sub_motion_search(MpegEncContext * s,
          int *mx_ptr, int *my_ptr, int dmin,
                                  int src_index, int ref_index,
                                  int size, int h)
{
    (*mx_ptr)<<=1;
    (*my_ptr)<<=1;
    return dmin;
}

int inline ff_get_mb_score(MpegEncContext * s, int mx, int my, int src_index,
                               int ref_index, int size, int h, int add_rate)
{
//    const int check_luma= s->dsp.me_sub_cmp != s->dsp.mb_cmp;
    MotionEstContext * const c= &s->me;
    const int penalty_factor= c->mb_penalty_factor;
    const int flags= c->mb_flags;
    const int qpel= flags & FLAG_QPEL;
    const int mask= 1+2*qpel;
    me_cmp_func cmp_sub, chroma_cmp_sub;
    int d;

    LOAD_COMMON

 //FIXME factorize

    cmp_sub= s->dsp.mb_cmp[size];
    chroma_cmp_sub= s->dsp.mb_cmp[size+1];

//    assert(!c->skip);
//    assert(c->avctx->me_sub_cmp != c->avctx->mb_cmp);

    d= cmp(s, mx>>(qpel+1), my>>(qpel+1), mx&mask, my&mask, size, h, ref_index, src_index, cmp_sub, chroma_cmp_sub, flags);
    //FIXME check cbp before adding penalty for (0,0) vector
    if(add_rate && (mx || my || size>0))
        d += (mv_penalty[mx - pred_x] + mv_penalty[my - pred_y])*penalty_factor;

    return d;
}

#define CHECK_QUARTER_MV(dx, dy, x, y)\
{\
    const int hx= 4*(x)+(dx);\
    const int hy= 4*(y)+(dy);\
    d= cmp(s, x, y, dx, dy, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);\
    d += (mv_penalty[hx - pred_x] + mv_penalty[hy - pred_y])*penalty_factor;\
    COPY3_IF_LT(dmin, d, bx, hx, by, hy)\
}

static int qpel_motion_search(MpegEncContext * s,
                                  int *mx_ptr, int *my_ptr, int dmin,
                                  int src_index, int ref_index,
                                  int size, int h)
{
    MotionEstContext * const c= &s->me;
    const int mx = *mx_ptr;
    const int my = *my_ptr;
    const int penalty_factor= c->sub_penalty_factor;
    const int map_generation= c->map_generation;
    const int subpel_quality= c->avctx->me_subpel_quality;
    uint32_t *map= c->map;
    me_cmp_func cmpf, chroma_cmpf;
    me_cmp_func cmp_sub, chroma_cmp_sub;

    LOAD_COMMON
    int flags= c->sub_flags;

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1]; //factorize FIXME
 //FIXME factorize

    cmp_sub= s->dsp.me_sub_cmp[size];
    chroma_cmp_sub= s->dsp.me_sub_cmp[size+1];

    if(c->skip){ //FIXME somehow move up (benchmark)
        *mx_ptr = 0;
        *my_ptr = 0;
        return dmin;
    }

    if(c->avctx->me_cmp != c->avctx->me_sub_cmp){
        dmin= cmp(s, mx, my, 0, 0, size, h, ref_index, src_index, cmp_sub, chroma_cmp_sub, flags);
        if(mx || my || size>0)
            dmin += (mv_penalty[4*mx - pred_x] + mv_penalty[4*my - pred_y])*penalty_factor;
    }

    if (mx > xmin && mx < xmax &&
        my > ymin && my < ymax) {
        int bx=4*mx, by=4*my;
        int d= dmin;
        int i, nx, ny;
        const int index= (my<<ME_MAP_SHIFT) + mx;
        const int t= score_map[(index-(1<<ME_MAP_SHIFT)  )&(ME_MAP_SIZE-1)];
        const int l= score_map[(index- 1                 )&(ME_MAP_SIZE-1)];
        const int r= score_map[(index+ 1                 )&(ME_MAP_SIZE-1)];
        const int b= score_map[(index+(1<<ME_MAP_SHIFT)  )&(ME_MAP_SIZE-1)];
        const int c= score_map[(index                    )&(ME_MAP_SIZE-1)];
        int best[8];
        int best_pos[8][2];

        memset(best, 64, sizeof(int)*8);
#if 1
        if(s->me.dia_size>=2){
            const int tl= score_map[(index-(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)];
            const int bl= score_map[(index+(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)];
            const int tr= score_map[(index-(1<<ME_MAP_SHIFT)+1)&(ME_MAP_SIZE-1)];
            const int br= score_map[(index+(1<<ME_MAP_SHIFT)+1)&(ME_MAP_SIZE-1)];

            for(ny= -3; ny <= 3; ny++){
                for(nx= -3; nx <= 3; nx++){
                    //FIXME this could overflow (unlikely though)
                    const int64_t t2= nx*nx*(tr + tl - 2*t) + 4*nx*(tr-tl) + 32*t;
                    const int64_t c2= nx*nx*( r +  l - 2*c) + 4*nx*( r- l) + 32*c;
                    const int64_t b2= nx*nx*(br + bl - 2*b) + 4*nx*(br-bl) + 32*b;
                    int score= (ny*ny*(b2 + t2 - 2*c2) + 4*ny*(b2 - t2) + 32*c2 + 512)>>10;
                    int i;

                    if((nx&3)==0 && (ny&3)==0) continue;

                    score += (mv_penalty[4*mx + nx - pred_x] + mv_penalty[4*my + ny - pred_y])*penalty_factor;

//                    if(nx&1) score-=1024*c->penalty_factor;
//                    if(ny&1) score-=1024*c->penalty_factor;

                    for(i=0; i<8; i++){
                        if(score < best[i]){
                            memmove(&best[i+1], &best[i], sizeof(int)*(7-i));
                            memmove(&best_pos[i+1][0], &best_pos[i][0], sizeof(int)*2*(7-i));
                            best[i]= score;
                            best_pos[i][0]= nx + 4*mx;
                            best_pos[i][1]= ny + 4*my;
                            break;
                        }
                    }
                }
            }
        }else{
            int tl;
            //FIXME this could overflow (unlikely though)
            const int cx = 4*(r - l);
            const int cx2= r + l - 2*c;
            const int cy = 4*(b - t);
            const int cy2= b + t - 2*c;
            int cxy;

            if(map[(index-(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)] == (my<<ME_MAP_MV_BITS) + mx + map_generation && 0){ //FIXME
                tl= score_map[(index-(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)];
            }else{
                tl= cmp(s, mx-1, my-1, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);//FIXME wrong if chroma me is different
            }

            cxy= 2*tl + (cx + cy)/4 - (cx2 + cy2) - 2*c;

            assert(16*cx2 + 4*cx + 32*c == 32*r);
            assert(16*cx2 - 4*cx + 32*c == 32*l);
            assert(16*cy2 + 4*cy + 32*c == 32*b);
            assert(16*cy2 - 4*cy + 32*c == 32*t);
            assert(16*cxy + 16*cy2 + 16*cx2 - 4*cy - 4*cx + 32*c == 32*tl);

            for(ny= -3; ny <= 3; ny++){
                for(nx= -3; nx <= 3; nx++){
                    //FIXME this could overflow (unlikely though)
                    int score= ny*nx*cxy + nx*nx*cx2 + ny*ny*cy2 + nx*cx + ny*cy + 32*c; //FIXME factor
                    int i;

                    if((nx&3)==0 && (ny&3)==0) continue;

                    score += 32*(mv_penalty[4*mx + nx - pred_x] + mv_penalty[4*my + ny - pred_y])*penalty_factor;
//                    if(nx&1) score-=32*c->penalty_factor;
  //                  if(ny&1) score-=32*c->penalty_factor;

                    for(i=0; i<8; i++){
                        if(score < best[i]){
                            memmove(&best[i+1], &best[i], sizeof(int)*(7-i));
                            memmove(&best_pos[i+1][0], &best_pos[i][0], sizeof(int)*2*(7-i));
                            best[i]= score;
                            best_pos[i][0]= nx + 4*mx;
                            best_pos[i][1]= ny + 4*my;
                            break;
                        }
                    }
                }
            }
        }
        for(i=0; i<subpel_quality; i++){
            nx= best_pos[i][0];
            ny= best_pos[i][1];
            CHECK_QUARTER_MV(nx&3, ny&3, nx>>2, ny>>2)
        }

#if 0
            const int tl= score_map[(index-(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)];
            const int bl= score_map[(index+(1<<ME_MAP_SHIFT)-1)&(ME_MAP_SIZE-1)];
            const int tr= score_map[(index-(1<<ME_MAP_SHIFT)+1)&(ME_MAP_SIZE-1)];
            const int br= score_map[(index+(1<<ME_MAP_SHIFT)+1)&(ME_MAP_SIZE-1)];
//            if(l < r && l < t && l < b && l < tl && l < bl && l < tr && l < br && bl < tl){
            if(tl<br){

//            nx= FFMAX(4*mx - bx, bx - 4*mx);
//            ny= FFMAX(4*my - by, by - 4*my);

            static int stats[7][7], count;
            count++;
            stats[4*mx - bx + 3][4*my - by + 3]++;
            if(256*256*256*64 % count ==0){
                for(i=0; i<49; i++){
                    if((i%7)==0) printf("\n");
                    printf("%6d ", stats[0][i]);
                }
                printf("\n");
            }
            }
#endif
#else

        CHECK_QUARTER_MV(2, 2, mx-1, my-1)
        CHECK_QUARTER_MV(0, 2, mx  , my-1)
        CHECK_QUARTER_MV(2, 2, mx  , my-1)
        CHECK_QUARTER_MV(2, 0, mx  , my  )
        CHECK_QUARTER_MV(2, 2, mx  , my  )
        CHECK_QUARTER_MV(0, 2, mx  , my  )
        CHECK_QUARTER_MV(2, 2, mx-1, my  )
        CHECK_QUARTER_MV(2, 0, mx-1, my  )

        nx= bx;
        ny= by;

        for(i=0; i<8; i++){
            int ox[8]= {0, 1, 1, 1, 0,-1,-1,-1};
            int oy[8]= {1, 1, 0,-1,-1,-1, 0, 1};
            CHECK_QUARTER_MV((nx + ox[i])&3, (ny + oy[i])&3, (nx + ox[i])>>2, (ny + oy[i])>>2)
        }
#endif
#if 0
        //outer ring
        CHECK_QUARTER_MV(1, 3, mx-1, my-1)
        CHECK_QUARTER_MV(1, 2, mx-1, my-1)
        CHECK_QUARTER_MV(1, 1, mx-1, my-1)
        CHECK_QUARTER_MV(2, 1, mx-1, my-1)
        CHECK_QUARTER_MV(3, 1, mx-1, my-1)
        CHECK_QUARTER_MV(0, 1, mx  , my-1)
        CHECK_QUARTER_MV(1, 1, mx  , my-1)
        CHECK_QUARTER_MV(2, 1, mx  , my-1)
        CHECK_QUARTER_MV(3, 1, mx  , my-1)
        CHECK_QUARTER_MV(3, 2, mx  , my-1)
        CHECK_QUARTER_MV(3, 3, mx  , my-1)
        CHECK_QUARTER_MV(3, 0, mx  , my  )
        CHECK_QUARTER_MV(3, 1, mx  , my  )
        CHECK_QUARTER_MV(3, 2, mx  , my  )
        CHECK_QUARTER_MV(3, 3, mx  , my  )
        CHECK_QUARTER_MV(2, 3, mx  , my  )
        CHECK_QUARTER_MV(1, 3, mx  , my  )
        CHECK_QUARTER_MV(0, 3, mx  , my  )
        CHECK_QUARTER_MV(3, 3, mx-1, my  )
        CHECK_QUARTER_MV(2, 3, mx-1, my  )
        CHECK_QUARTER_MV(1, 3, mx-1, my  )
        CHECK_QUARTER_MV(1, 2, mx-1, my  )
        CHECK_QUARTER_MV(1, 1, mx-1, my  )
        CHECK_QUARTER_MV(1, 0, mx-1, my  )
#endif
        assert(bx >= xmin*4 && bx <= xmax*4 && by >= ymin*4 && by <= ymax*4);

        *mx_ptr = bx;
        *my_ptr = by;
    }else{
        *mx_ptr =4*mx;
        *my_ptr =4*my;
    }

    return dmin;
}


#define CHECK_MV(x,y)\
{\
    const int key= ((y)<<ME_MAP_MV_BITS) + (x) + map_generation;\
    const int index= (((y)<<ME_MAP_SHIFT) + (x))&(ME_MAP_SIZE-1);\
    assert((x) >= xmin);\
    assert((x) <= xmax);\
    assert((y) >= ymin);\
    assert((y) <= ymax);\
/*printf("check_mv %d %d\n", x, y);*/\
    if(map[index]!=key){\
        d= cmp(s, x, y, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);\
        map[index]= key;\
        score_map[index]= d;\
        d += (mv_penalty[((x)<<shift)-pred_x] + mv_penalty[((y)<<shift)-pred_y])*penalty_factor;\
/*printf("score:%d\n", d);*/\
        COPY3_IF_LT(dmin, d, best[0], x, best[1], y)\
    }\
}

#define CHECK_CLIPED_MV(ax,ay)\
{\
    const int x= ax;\
    const int y= ay;\
    const int x2= FFMAX(xmin, FFMIN(x, xmax));\
    const int y2= FFMAX(ymin, FFMIN(y, ymax));\
    CHECK_MV(x2, y2)\
}

#define CHECK_MV_DIR(x,y,new_dir)\
{\
    const int key= ((y)<<ME_MAP_MV_BITS) + (x) + map_generation;\
    const int index= (((y)<<ME_MAP_SHIFT) + (x))&(ME_MAP_SIZE-1);\
/*printf("check_mv_dir %d %d %d\n", x, y, new_dir);*/\
    if(map[index]!=key){\
        d= cmp(s, x, y, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);\
        map[index]= key;\
        score_map[index]= d;\
        d += (mv_penalty[((x)<<shift)-pred_x] + mv_penalty[((y)<<shift)-pred_y])*penalty_factor;\
/*printf("score:%d\n", d);*/\
        if(d<dmin){\
            best[0]=x;\
            best[1]=y;\
            dmin=d;\
            next_dir= new_dir;\
        }\
    }\
}

#define check(x,y,S,v)\
if( (x)<(xmin<<(S)) ) printf("%d %d %d %d %d xmin" #v, xmin, (x), (y), s->mb_x, s->mb_y);\
if( (x)>(xmax<<(S)) ) printf("%d %d %d %d %d xmax" #v, xmax, (x), (y), s->mb_x, s->mb_y);\
if( (y)<(ymin<<(S)) ) printf("%d %d %d %d %d ymin" #v, ymin, (x), (y), s->mb_x, s->mb_y);\
if( (y)>(ymax<<(S)) ) printf("%d %d %d %d %d ymax" #v, ymax, (x), (y), s->mb_x, s->mb_y);\

#define LOAD_COMMON2\
    uint32_t *map= c->map;\
    const int qpel= flags&FLAG_QPEL;\
    const int shift= 1+qpel;\

static always_inline int small_diamond_search(MpegEncContext * s, int *best, int dmin,
                                       int src_index, int ref_index, int const penalty_factor,
                                       int size, int h, int flags)
{
    MotionEstContext * const c= &s->me;
    me_cmp_func cmpf, chroma_cmpf;
    int next_dir=-1;
    LOAD_COMMON
    LOAD_COMMON2
    int map_generation= c->map_generation;

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    { /* ensure that the best point is in the MAP as h/qpel refinement needs it */
        const int key= (best[1]<<ME_MAP_MV_BITS) + best[0] + map_generation;
        const int index= ((best[1]<<ME_MAP_SHIFT) + best[0])&(ME_MAP_SIZE-1);
        if(map[index]!=key){ //this will be executed only very rarey
            score_map[index]= cmp(s, best[0], best[1], 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);
            map[index]= key;
        }
    }

    for(;;){
        int d;
        const int dir= next_dir;
        const int x= best[0];
        const int y= best[1];
        next_dir=-1;

//printf("%d", dir);
        if(dir!=2 && x>xmin) CHECK_MV_DIR(x-1, y  , 0)
        if(dir!=3 && y>ymin) CHECK_MV_DIR(x  , y-1, 1)
        if(dir!=0 && x<xmax) CHECK_MV_DIR(x+1, y  , 2)
        if(dir!=1 && y<ymax) CHECK_MV_DIR(x  , y+1, 3)

        if(next_dir==-1){
            return dmin;
        }
    }
}

static int funny_diamond_search(MpegEncContext * s, int *best, int dmin,
                                       int src_index, int ref_index, int const penalty_factor,
                                       int size, int h, int flags)
{
    MotionEstContext * const c= &s->me;
    me_cmp_func cmpf, chroma_cmpf;
    int dia_size;
    LOAD_COMMON
    LOAD_COMMON2
    int map_generation= c->map_generation;

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    for(dia_size=1; dia_size<=4; dia_size++){
        int dir;
        const int x= best[0];
        const int y= best[1];

        if(dia_size&(dia_size-1)) continue;

        if(   x + dia_size > xmax
           || x - dia_size < xmin
           || y + dia_size > ymax
           || y - dia_size < ymin)
           continue;

        for(dir= 0; dir<dia_size; dir+=2){
            int d;

            CHECK_MV(x + dir           , y + dia_size - dir);
            CHECK_MV(x + dia_size - dir, y - dir           );
            CHECK_MV(x - dir           , y - dia_size + dir);
            CHECK_MV(x - dia_size + dir, y + dir           );
        }

        if(x!=best[0] || y!=best[1])
            dia_size=0;
#if 0
{
int dx, dy, i;
static int stats[8*8];
dx= ABS(x-best[0]);
dy= ABS(y-best[1]);
if(dy>dx){
    dx^=dy; dy^=dx; dx^=dy;
}
stats[dy*8 + dx] ++;
if(256*256*256*64 % (stats[0]+1)==0){
    for(i=0; i<64; i++){
        if((i&7)==0) printf("\n");
        printf("%8d ", stats[i]);
    }
    printf("\n");
}
}
#endif
    }
    return dmin;
}

#define SAB_CHECK_MV(ax,ay)\
{\
    const int key= ((ay)<<ME_MAP_MV_BITS) + (ax) + map_generation;\
    const int index= (((ay)<<ME_MAP_SHIFT) + (ax))&(ME_MAP_SIZE-1);\
/*printf("sab check %d %d\n", ax, ay);*/\
    if(map[index]!=key){\
        d= cmp(s, ax, ay, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);\
        map[index]= key;\
        score_map[index]= d;\
        d += (mv_penalty[((ax)<<shift)-pred_x] + mv_penalty[((ay)<<shift)-pred_y])*penalty_factor;\
/*printf("score: %d\n", d);*/\
        if(d < minima[minima_count-1].height){\
            int j=0;\
            \
            while(d >= minima[j].height) j++;\
\
            memmove(&minima [j+1], &minima [j], (minima_count - j - 1)*sizeof(Minima));\
\
            minima[j].checked= 0;\
            minima[j].height= d;\
            minima[j].x= ax;\
            minima[j].y= ay;\
            \
            i=-1;\
            continue;\
        }\
    }\
}

#define MAX_SAB_SIZE ME_MAP_SIZE
static int sab_diamond_search(MpegEncContext * s, int *best, int dmin,
                                       int src_index, int ref_index, int const penalty_factor,
                                       int size, int h, int flags)
{
    MotionEstContext * const c= &s->me;
    me_cmp_func cmpf, chroma_cmpf;
    Minima minima[MAX_SAB_SIZE];
    const int minima_count= ABS(c->dia_size);
    int i, j;
    LOAD_COMMON
    LOAD_COMMON2
    int map_generation= c->map_generation;

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    for(j=i=0; i<ME_MAP_SIZE; i++){
        uint32_t key= map[i];

        key += (1<<(ME_MAP_MV_BITS-1)) + (1<<(2*ME_MAP_MV_BITS-1));

        if((key&((-1)<<(2*ME_MAP_MV_BITS))) != map_generation) continue;

        assert(j<MAX_SAB_SIZE); //max j = number of predictors

        minima[j].height= score_map[i];
        minima[j].x= key & ((1<<ME_MAP_MV_BITS)-1); key>>=ME_MAP_MV_BITS;
        minima[j].y= key & ((1<<ME_MAP_MV_BITS)-1);
        minima[j].x-= (1<<(ME_MAP_MV_BITS-1));
        minima[j].y-= (1<<(ME_MAP_MV_BITS-1));
        minima[j].checked=0;
        if(minima[j].x || minima[j].y)
            minima[j].height+= (mv_penalty[((minima[j].x)<<shift)-pred_x] + mv_penalty[((minima[j].y)<<shift)-pred_y])*penalty_factor;

        j++;
    }

    qsort(minima, j, sizeof(Minima), minima_cmp);

    for(; j<minima_count; j++){
        minima[j].height=256*256*256*64;
        minima[j].checked=0;
        minima[j].x= minima[j].y=0;
    }

    for(i=0; i<minima_count; i++){
        const int x= minima[i].x;
        const int y= minima[i].y;
        int d;

        if(minima[i].checked) continue;

        if(   x >= xmax || x <= xmin
           || y >= ymax || y <= ymin)
           continue;

        SAB_CHECK_MV(x-1, y)
        SAB_CHECK_MV(x+1, y)
        SAB_CHECK_MV(x  , y-1)
        SAB_CHECK_MV(x  , y+1)

        minima[i].checked= 1;
    }

    best[0]= minima[0].x;
    best[1]= minima[0].y;
    dmin= minima[0].height;

    if(   best[0] < xmax && best[0] > xmin
       && best[1] < ymax && best[1] > ymin){
        int d;
        //ensure that the refernece samples for hpel refinement are in the map
        CHECK_MV(best[0]-1, best[1])
        CHECK_MV(best[0]+1, best[1])
        CHECK_MV(best[0], best[1]-1)
        CHECK_MV(best[0], best[1]+1)
    }
    return dmin;
}

static int var_diamond_search(MpegEncContext * s, int *best, int dmin,
                                       int src_index, int ref_index, int const penalty_factor,
                                       int size, int h, int flags)
{
    MotionEstContext * const c= &s->me;
    me_cmp_func cmpf, chroma_cmpf;
    int dia_size;
    LOAD_COMMON
    LOAD_COMMON2
    int map_generation= c->map_generation;

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    for(dia_size=1; dia_size<=c->dia_size; dia_size++){
        int dir, start, end;
        const int x= best[0];
        const int y= best[1];

        start= FFMAX(0, y + dia_size - ymax);
        end  = FFMIN(dia_size, xmax - x + 1);
        for(dir= start; dir<end; dir++){
            int d;

//check(x + dir,y + dia_size - dir,0, a0)
            CHECK_MV(x + dir           , y + dia_size - dir);
        }

        start= FFMAX(0, x + dia_size - xmax);
        end  = FFMIN(dia_size, y - ymin + 1);
        for(dir= start; dir<end; dir++){
            int d;

//check(x + dia_size - dir, y - dir,0, a1)
            CHECK_MV(x + dia_size - dir, y - dir           );
        }

        start= FFMAX(0, -y + dia_size + ymin );
        end  = FFMIN(dia_size, x - xmin + 1);
        for(dir= start; dir<end; dir++){
            int d;

//check(x - dir,y - dia_size + dir,0, a2)
            CHECK_MV(x - dir           , y - dia_size + dir);
        }

        start= FFMAX(0, -x + dia_size + xmin );
        end  = FFMIN(dia_size, ymax - y + 1);
        for(dir= start; dir<end; dir++){
            int d;

//check(x - dia_size + dir, y + dir,0, a3)
            CHECK_MV(x - dia_size + dir, y + dir           );
        }

        if(x!=best[0] || y!=best[1])
            dia_size=0;
#if 0
{
int dx, dy, i;
static int stats[8*8];
dx= ABS(x-best[0]);
dy= ABS(y-best[1]);
stats[dy*8 + dx] ++;
if(256*256*256*64 % (stats[0]+1)==0){
    for(i=0; i<64; i++){
        if((i&7)==0) printf("\n");
        printf("%6d ", stats[i]);
    }
    printf("\n");
}
}
#endif
    }
    return dmin;
}

static always_inline int diamond_search(MpegEncContext * s, int *best, int dmin,
                                       int src_index, int ref_index, int const penalty_factor,
                                       int size, int h, int flags){
    MotionEstContext * const c= &s->me;
    if(c->dia_size==-1)
        return funny_diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);
    else if(c->dia_size<-1)
        return   sab_diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);
    else if(c->dia_size<2)
        return small_diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);
    else
        return   var_diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);
}

static always_inline int epzs_motion_search_internal(MpegEncContext * s, int *mx_ptr, int *my_ptr,
                             int P[10][2], int src_index, int ref_index, int16_t (*last_mv)[2],
                             int ref_mv_scale, int flags, int size, int h)
{
    MotionEstContext * const c= &s->me;
    int best[2]={0, 0};
    int d, dmin;
    int map_generation;
    int penalty_factor;
    const int ref_mv_stride= s->mb_stride; //pass as arg  FIXME
    const int ref_mv_xy= s->mb_x + s->mb_y*ref_mv_stride; //add to last_mv beforepassing FIXME
    me_cmp_func cmpf, chroma_cmpf;

    LOAD_COMMON
    LOAD_COMMON2

    if(c->pre_pass){
        penalty_factor= c->pre_penalty_factor;
        cmpf= s->dsp.me_pre_cmp[size];
        chroma_cmpf= s->dsp.me_pre_cmp[size+1];
    }else{
        penalty_factor= c->penalty_factor;
        cmpf= s->dsp.me_cmp[size];
        chroma_cmpf= s->dsp.me_cmp[size+1];
    }

    map_generation= update_map_generation(c);

    assert(cmpf);
    dmin= cmp(s, 0, 0, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, flags);
    map[0]= map_generation;
    score_map[0]= dmin;

    /* first line */
    if (s->first_slice_line) {
        CHECK_MV(P_LEFT[0]>>shift, P_LEFT[1]>>shift)
        CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                        (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
    }else{
        if(dmin<((h*h*s->avctx->mv0_threshold)>>8)
                    && ( P_LEFT[0]    |P_LEFT[1]
                        |P_TOP[0]     |P_TOP[1]
                        |P_TOPRIGHT[0]|P_TOPRIGHT[1])==0){
            *mx_ptr= 0;
            *my_ptr= 0;
            c->skip=1;
            return dmin;
        }
        CHECK_MV(P_MEDIAN[0]>>shift, P_MEDIAN[1]>>shift)
        if(dmin>h*h*2){
            CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
            CHECK_MV(P_LEFT[0]    >>shift, P_LEFT[1]    >>shift)
            CHECK_MV(P_TOP[0]     >>shift, P_TOP[1]     >>shift)
            CHECK_MV(P_TOPRIGHT[0]>>shift, P_TOPRIGHT[1]>>shift)
        }
    }
    if(dmin>h*h*4){
        if(c->pre_pass){
            CHECK_CLIPED_MV((last_mv[ref_mv_xy-1][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy-1][1]*ref_mv_scale + (1<<15))>>16)
            if(!s->first_slice_line)
                CHECK_CLIPED_MV((last_mv[ref_mv_xy-ref_mv_stride][0]*ref_mv_scale + (1<<15))>>16,
                                (last_mv[ref_mv_xy-ref_mv_stride][1]*ref_mv_scale + (1<<15))>>16)
        }else{
            CHECK_CLIPED_MV((last_mv[ref_mv_xy+1][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy+1][1]*ref_mv_scale + (1<<15))>>16)
            if(s->mb_y+1<s->end_mb_y)  //FIXME replace at least with last_slice_line
                CHECK_CLIPED_MV((last_mv[ref_mv_xy+ref_mv_stride][0]*ref_mv_scale + (1<<15))>>16,
                                (last_mv[ref_mv_xy+ref_mv_stride][1]*ref_mv_scale + (1<<15))>>16)
        }
    }

    if(c->avctx->last_predictor_count){
        const int count= c->avctx->last_predictor_count;
        const int xstart= FFMAX(0, s->mb_x - count);
        const int ystart= FFMAX(0, s->mb_y - count);
        const int xend= FFMIN(s->mb_width , s->mb_x + count + 1);
        const int yend= FFMIN(s->mb_height, s->mb_y + count + 1);
        int mb_y;

        for(mb_y=ystart; mb_y<yend; mb_y++){
            int mb_x;
            for(mb_x=xstart; mb_x<xend; mb_x++){
                const int xy= mb_x + 1 + (mb_y + 1)*ref_mv_stride;
                int mx= (last_mv[xy][0]*ref_mv_scale + (1<<15))>>16;
                int my= (last_mv[xy][1]*ref_mv_scale + (1<<15))>>16;

                if(mx>xmax || mx<xmin || my>ymax || my<ymin) continue;
                CHECK_MV(mx,my)
            }
        }
    }

//check(best[0],best[1],0, b0)
    dmin= diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);

//check(best[0],best[1],0, b1)
    *mx_ptr= best[0];
    *my_ptr= best[1];

//    printf("%d %d %d \n", best[0], best[1], dmin);
    return dmin;
}

//this function is dedicated to the braindamaged gcc
inline int ff_epzs_motion_search(MpegEncContext * s, int *mx_ptr, int *my_ptr,
                             int P[10][2], int src_index, int ref_index, int16_t (*last_mv)[2],
                             int ref_mv_scale, int size, int h)
{
    MotionEstContext * const c= &s->me;
//FIXME convert other functions in the same way if faster
    if(c->flags==0 && h==16 && size==0){
        return epzs_motion_search_internal(s, mx_ptr, my_ptr, P, src_index, ref_index, last_mv, ref_mv_scale, 0, 0, 16);
//    case FLAG_QPEL:
//        return epzs_motion_search_internal(s, mx_ptr, my_ptr, P, src_index, ref_index, last_mv, ref_mv_scale, FLAG_QPEL);
    }else{
        return epzs_motion_search_internal(s, mx_ptr, my_ptr, P, src_index, ref_index, last_mv, ref_mv_scale, c->flags, size, h);
    }
}

static int epzs_motion_search4(MpegEncContext * s,
                             int *mx_ptr, int *my_ptr, int P[10][2],
                             int src_index, int ref_index, int16_t (*last_mv)[2],
                             int ref_mv_scale)
{
    MotionEstContext * const c= &s->me;
    int best[2]={0, 0};
    int d, dmin;
    int map_generation;
    const int penalty_factor= c->penalty_factor;
    const int size=1;
    const int h=8;
    const int ref_mv_stride= s->mb_stride;
    const int ref_mv_xy= s->mb_x + s->mb_y *ref_mv_stride;
    me_cmp_func cmpf, chroma_cmpf;
    LOAD_COMMON
    int flags= c->flags;
    LOAD_COMMON2

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    map_generation= update_map_generation(c);

    dmin = 1000000;
//printf("%d %d %d %d //",xmin, ymin, xmax, ymax);
    /* first line */
    if (s->first_slice_line) {
        CHECK_MV(P_LEFT[0]>>shift, P_LEFT[1]>>shift)
        CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                        (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
        CHECK_MV(P_MV1[0]>>shift, P_MV1[1]>>shift)
    }else{
        CHECK_MV(P_MV1[0]>>shift, P_MV1[1]>>shift)
        //FIXME try some early stop
        if(dmin>64*2){
            CHECK_MV(P_MEDIAN[0]>>shift, P_MEDIAN[1]>>shift)
            CHECK_MV(P_LEFT[0]>>shift, P_LEFT[1]>>shift)
            CHECK_MV(P_TOP[0]>>shift, P_TOP[1]>>shift)
            CHECK_MV(P_TOPRIGHT[0]>>shift, P_TOPRIGHT[1]>>shift)
            CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
        }
    }
    if(dmin>64*4){
        CHECK_CLIPED_MV((last_mv[ref_mv_xy+1][0]*ref_mv_scale + (1<<15))>>16,
                        (last_mv[ref_mv_xy+1][1]*ref_mv_scale + (1<<15))>>16)
        if(s->mb_y+1<s->end_mb_y)  //FIXME replace at least with last_slice_line
            CHECK_CLIPED_MV((last_mv[ref_mv_xy+ref_mv_stride][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy+ref_mv_stride][1]*ref_mv_scale + (1<<15))>>16)
    }

    dmin= diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);

    *mx_ptr= best[0];
    *my_ptr= best[1];

//    printf("%d %d %d \n", best[0], best[1], dmin);
    return dmin;
}

//try to merge with above FIXME (needs PSNR test)
static int epzs_motion_search2(MpegEncContext * s,
                             int *mx_ptr, int *my_ptr, int P[10][2],
                             int src_index, int ref_index, int16_t (*last_mv)[2],
                             int ref_mv_scale)
{
    MotionEstContext * const c= &s->me;
    int best[2]={0, 0};
    int d, dmin;
    int map_generation;
    const int penalty_factor= c->penalty_factor;
    const int size=0; //FIXME pass as arg
    const int h=8;
    const int ref_mv_stride= s->mb_stride;
    const int ref_mv_xy= s->mb_x + s->mb_y *ref_mv_stride;
    me_cmp_func cmpf, chroma_cmpf;
    LOAD_COMMON
    int flags= c->flags;
    LOAD_COMMON2

    cmpf= s->dsp.me_cmp[size];
    chroma_cmpf= s->dsp.me_cmp[size+1];

    map_generation= update_map_generation(c);

    dmin = 1000000;
//printf("%d %d %d %d //",xmin, ymin, xmax, ymax);
    /* first line */
    if (s->first_slice_line) {
        CHECK_MV(P_LEFT[0]>>shift, P_LEFT[1]>>shift)
        CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                        (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
        CHECK_MV(P_MV1[0]>>shift, P_MV1[1]>>shift)
    }else{
        CHECK_MV(P_MV1[0]>>shift, P_MV1[1]>>shift)
        //FIXME try some early stop
        if(dmin>64*2){
            CHECK_MV(P_MEDIAN[0]>>shift, P_MEDIAN[1]>>shift)
            CHECK_MV(P_LEFT[0]>>shift, P_LEFT[1]>>shift)
            CHECK_MV(P_TOP[0]>>shift, P_TOP[1]>>shift)
            CHECK_MV(P_TOPRIGHT[0]>>shift, P_TOPRIGHT[1]>>shift)
            CHECK_CLIPED_MV((last_mv[ref_mv_xy][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy][1]*ref_mv_scale + (1<<15))>>16)
        }
    }
    if(dmin>64*4){
        CHECK_CLIPED_MV((last_mv[ref_mv_xy+1][0]*ref_mv_scale + (1<<15))>>16,
                        (last_mv[ref_mv_xy+1][1]*ref_mv_scale + (1<<15))>>16)
        if(s->mb_y+1<s->end_mb_y)  //FIXME replace at least with last_slice_line
            CHECK_CLIPED_MV((last_mv[ref_mv_xy+ref_mv_stride][0]*ref_mv_scale + (1<<15))>>16,
                            (last_mv[ref_mv_xy+ref_mv_stride][1]*ref_mv_scale + (1<<15))>>16)
    }

    dmin= diamond_search(s, best, dmin, src_index, ref_index, penalty_factor, size, h, flags);

    *mx_ptr= best[0];
    *my_ptr= best[1];

//    printf("%d %d %d \n", best[0], best[1], dmin);
    return dmin;
}
