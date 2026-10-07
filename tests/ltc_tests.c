/* Timecode decoder tests (src/ltc-decode.h) with synthetic LTC audio - run on a computer, see tests/run.sh */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ltc-decode.h"   /* src/ltc-decode.h */

/* generator: biphase-mark LTC, returns samples; start tc h:m:s:f at nominal fps (integer) */
typedef struct { double fps_real; int nominal; int rate; double amp; double dc; double noise; int invert; double clip; double jitter; } cfg_t;
static double rnd(){ return rand()/(double)RAND_MAX; }
int gen(int16_t *out, int max, cfg_t c, int h,int m,int s,int f, int nframes, int *labels)
{
    int n=0; int level=1; double pos=0.0;
    double spb = c.rate/(c.fps_real*80.0);
    for (int fr=0; fr<nframes; fr++) {
        uint8_t b[80]; memset(b,0,80);
        #define SETF(base,val,nb) for(int i=0;i<(nb);i++) b[(base)+i]=((val)>>i)&1;
        SETF(0,f%10,4); SETF(8,f/10,2); SETF(16,s%10,4); SETF(24,s/10,3);
        SETF(32,m%10,4); SETF(40,m/10,3); SETF(48,h%10,4); SETF(56,h/10,2);
        static const uint8_t sync[16]={0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1};
        for(int i=0;i<16;i++) b[64+i]=sync[i];
        /* bit 27 parity so that number of ones is even (phase correction) - best effort */
        int ones=0; for(int i=0;i<80;i++) ones+=b[i]; if (ones&1) b[27]=1;
        for(int i=0;i<80;i++){
            double j = 1.0 + c.jitter*(rnd()-0.5);
            level=-level; double edge1=pos; 
            double half = spb/2*j, full=spb*j;
            int segs = b[i]?2:1;
            for (int sg=0; sg<segs; sg++) {
                double len = b[i]? half: full;
                int cnt=(int)(pos+len) - (int)pos; 
                for(int k=0;k<cnt && n<max;k++){
                    double v=level*c.amp*(c.invert?-1:1)+c.dc+c.noise*(rnd()*2-1);
                    if (v >  c.clip) v =  c.clip;
                    if (v < -c.clip) v = -c.clip;
                    if (v >  32767) v =  32767;
                    if (v < -32768) v = -32768;
                    out[n]=(int16_t)v;
                    if (labels) labels[n]=fr;
                    n++;
                }
                pos+=len;
                if (sg==0 && segs==2) level=-level;
            }
            (void)edge1;
        }
        f++; if (f>=c.nominal){f=0;s++;} if(s>=60){s=0;m++;} if(m>=60){m=0;h++;} if(h>=24)h=0;
    }
    return n;
}

static int run(const char*name, cfg_t c, int decoder_fps_x1000, int stereo, int chunk)
{
    static int16_t mono[48000*12]; static int16_t st[48000*12*2];
    int nframes = (int)(c.fps_real*8);
    int n = gen(mono, sizeof(mono)/2, c, 1,12,44,5, nframes, NULL);
    const int16_t *src=mono; int stride=1;
    if (stereo) { for(int i=0;i<n;i++){ st[2*i]=mono[i]; st[2*i+1]=(int16_t)(8000*sin(i*0.05)); } src=st; stride=2; }
    ltc_t t; ltc_init(&t,c.rate,decoder_fps_x1000);
    int prev=-1, good=0, bad=0, first_h=-1;
    int idx_total=0;
    for (int p=0;p<n;p+=chunk){
        int k = (n-p<chunk)?n-p:chunk;
        uint32_t before=t.frames_ok;
        ltc_feed(&t, src+p*stride, k, stride);
        (void)before;
        idx_total+=k;
        if (t.frames_ok!=before) {
            int idx = ((t.h*60+t.m)*60+t.s)*c.nominal+t.f;
            if (prev>=0) { int dlt=idx-prev; if (dlt>=1 && dlt<=2 && t.frames_ok==before+ (uint32_t)dlt) good++; else if (dlt==(int)(t.frames_ok-before)) good++; else bad++; }
            else first_h=idx;
            prev=idx;
        }
    }
    int expect_start = ((1*60+12)*60+44)*c.nominal+5;
    printf("%-44s frames_ok=%4u of %d  first=%s  bad_steps=%d  last=%02d:%02d:%02d:%02d\n", name, t.frames_ok, nframes,
           (first_h>=expect_start && first_h<=expect_start+3)?"ok":"WRONG", bad, t.h,t.m,t.s,t.f);
    return (t.frames_ok >= (unsigned)(nframes-3) && bad==0 && first_h>=expect_start && first_h<=expect_start+3)?0:1;
}

int main(){
    srand(5); int fail=0;
    cfg_t base={24.0,24,48000,12000,0,0,0,32767,0};
    fail+=run("24 fps clean, mono, 2000-sample chunks", base, 24000,0,2000);
    cfg_t c=base; c.fps_real=23.976; fail+=run("23.976 fps, decoder told 24", c, 24000,0,2000);
    c=base; c.fps_real=23.983; fail+=run("23.983 fps (camera clock)", c, 23983,0,2002);
    c=base; c.fps_real=25; c.nominal=25; fail+=run("25 fps tc, decoder hint 24 (mismatch)", c, 24000,0,2000);
    c=base; c.fps_real=30; c.nominal=30; fail+=run("30 fps tc, decoder hint 24", c, 24000,0,2000);
    c=base; c.invert=1; fail+=run("inverted polarity", c, 24000,0,2000);
    c=base; c.dc=4000; fail+=run("DC offset 4000", c, 24000,0,2000);
    c=base; c.amp=900; c.noise=250; fail+=run("low level 900 + noise 250", c, 24000,0,2000);
    c=base; c.amp=200; c.noise=40; fail+=run("very low level 200 + noise 40", c, 24000,0,2000);
    c=base; c.amp=60000; c.clip=32767; fail+=run("hard clipped (square, full scale)", c, 24000,0,2000);
    c=base; c.jitter=0.12; fail+=run("12% edge jitter", c, 24000,0,2000);
    c=base; fail+=run("stereo interleaved, other ch = tone", base, 24000,1,2000);
    c=base; c.rate=44100; fail+=run("44.1 kHz", c, 24000,0,1837);
    c=base; fail+=run("tiny chunks (7 samples)", base, 24000,0,7);
    /* silence / noise must not decode */
    { ltc_t t; ltc_init(&t,48000,24000); static int16_t z[48000*3]; for(int i=0;i<48000*3;i++) z[i]=(int16_t)(rand()%400-200);
      ltc_feed(&t,z,48000*3,1); printf("%-44s frames_ok=%u (expect 0)\n","pure noise +-200",t.frames_ok); if(t.frames_ok) fail++; }
    { ltc_t t; ltc_init(&t,48000,24000); static int16_t z[48000*3]; for(int i=0;i<48000*3;i++) z[i]=(int16_t)(8000*sin(i*0.07));
      ltc_feed(&t,z,48000*3,1); printf("%-44s frames_ok=%u (expect 0)\n","steady 1.07 kHz tone",t.frames_ok); if(t.frames_ok) fail++; }
    { ltc_t t; ltc_init(&t,48000,24000); static int16_t z[48000*3]; for(int i=0;i<48000*3;i++) z[i]=(int16_t)((rand()%30000)-15000);
      ltc_feed(&t,z,48000*3,1); printf("%-44s frames_ok=%u (expect 0)\n","loud white noise",t.frames_ok); if(t.frames_ok) fail++; }
    printf("\n%s (%d failing)\n", fail?"SOME FAILED":"ALL PASSED", fail);
    return fail;
}
