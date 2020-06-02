
#include "global.h"
#include "htpa32_def.h"
#include "htpa32_main.h"
#include "htpa32_table.h"

#define TA_MIN      (2782)
#define TA_MAX      (3382)
#define TA_STEP     (100)
#define TA_NUM      (7)

#define VOUT_MIN    (-128)
#define VOUT_STEP   (64)
#define VOUT_MAX    (576)
#define VOUT_NUM    (12)


static const uint16_t temptable[][TA_NUM] = {
    /*   2773  2882  2982  3082  3182  3282  3382 */
    {2481, 2612, 2735 - 10, 2852 + 36, 2964 + 20, 3073, 3180},  /* -128 */
    {2642, 2755, 2865 - 10, 2972 + 36, 3078 + 20, 3182, 3285},  /* -64  */
    {2782, 2882, 2982 - 10, 3082 + 36, 3182 + 20, 3282, 3382},  /* 0    */
    {2906, 2996, 3089 - 10, 3183 + 36, 3278 + 20, 3375, 3473},  /* 64   */
    {3019, 3101, 3187 - 10, 3276 + 36, 3368 + 20, 3462, 3558},  /* 128  */
    {3121, 3197, 3278 - 10, 3363 + 36, 3452 + 20, 3544, 3638},  /* 192  */
    {3216, 3286, 3363 - 10, 3445 + 36, 3531 + 20, 3621, 3715},  /* 256  */
    {3305, 3370, 3443 - 10, 3522 + 36, 3606 + 20, 3695, 3787},  /* 320  */
    {3387, 3449, 3519 - 10, 3595 + 36, 3677 + 20, 3764, 3856},  /* 384  */
    {3465, 3524, 3590 - 10, 3664 + 36, 3745 + 20, 3831, 3922},  /* 448  */
    {3539, 3595, 3659 - 10, 3731 + 36, 3810 + 20, 3895, 3986},  /* 512  */
    {3609, 3662, 3724 - 10, 3794 + 36, 3872 + 20, 3957, 4047},  /* 576  */
};

static int16_t g_offset = 0;
static int16_t g_usroffset = 0;
void Htpa32_SetOffset(int16_t offset)
{
    g_offset  = offset;
}

void Htpa32_SetUserOffset(int16_t offset)
{
    g_usroffset  = offset;
}

int16_t Htpa32_GetUserOffset(void)
{
    return g_usroffset  ;
}


void Htpa32_SearchTbl(struct OutData_t *output, int32_t *inbuf)
{
    int i;
    int16_t x0, y0;
    uint16_t a00, a01, a10, a11;
    uint16_t ta0, ta1;
    int16_t vout0, vout1;
    int16_t vout;
    uint16_t b0, b1;

    int16_t *outbuf;
    uint16_t ta;

    outbuf = output->out;
    ta = output->ta;

    if (ta < TA_MIN) {
        for (i = 0; i < 1024; i++) {
            output->sample[i] = (int16_t)inbuf[i];
            outbuf[i]  = 0;
        }
        return ;
    }

    if (ta > TA_MAX) ta = TA_MAX;

    x0 = (ta - TA_MIN) / TA_STEP;
    ta0 = x0 * TA_STEP + TA_MIN;

    ta1 =  ta0 + TA_STEP;
    if (ta1 > TA_MAX)
        ta1 = ta0;

    for (i = 0; i < 1024; i++) {
        output->sample[i] = (int16_t)inbuf[i];
        if (inbuf[i] < VOUT_MIN) {
            vout = VOUT_MIN;
        } else {
            vout = (inbuf[i] > VOUT_MAX) ? VOUT_MAX : inbuf[i];
        }

        y0 = (vout - VOUT_MIN) / VOUT_STEP;
        vout0 = y0 * VOUT_STEP + VOUT_MIN;
        vout1 = vout0 + VOUT_STEP;

        if (vout1 > VOUT_MAX)
            vout1 = vout0;

        a00 = temptable[y0][x0]  ;
        if ((x0 + 1) < TA_NUM)  {
            a01 = temptable[y0][x0 + 1]  ;
        } else {
            a01 = a00;
        }

        if ((y0 + 1) < VOUT_NUM) {
            a10 = temptable[y0 + 1][x0]  ;
            if ((x0 + 1) < TA_NUM)  {
                a11 = temptable[y0 + 1][x0 + 1]  ;
            } else {
                a11 = a10;
            }
        } else {
            a10 = a00;
            a11 = a01;
        }

        b0 = (a01 - a00) * (ta - ta0) / TA_STEP + a00;
        b1 = (a11 - a10) * (ta - ta0) / TA_STEP + a10;

        outbuf[i] = ((b1 - b0) * (vout - vout0) / VOUT_STEP + b0 + g_offset) * 10 - 27315 + g_usroffset ;
    }

    return ;
}


