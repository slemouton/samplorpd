/*
 slm1 utility global routines Library
 */

#include <stdlib.h>
#include "max_types.h"
#include "m_pd.h"
#include "math.h"
#include "slm1.h"

/*
 * atom utility function
 */
#if 0 
float slm1_get_value(struct atom *a)
{
    switch (a->a_type)
    {
        case A_FLOAT: return a->a_w.w_float;
        case A_LONG:  return a->a_w.w_long;
        case A_SYM:   if (*a->a_w.w_sym->s_name == '#') break;
        default:      error("slm-1: illegal init argument");
    }
    return 0;
}

t_symbol *slm1_get_symbol(struct atom *a)
{
    switch (a->a_type)
    {
        case A_SYM:   return a->a_w.w_sym;
            // default:      error("coro~: illegal init argument");
    }
    return 0;
}
#endif

/* met ˆ zero un vecteur */

void vzero_f( float *f, int n)
{
    float *p;
    
    p = f;
    while (n--) *p++ = 0.;
}

void vzero( double *f, int n)
{
    double *p;
    
    p = f;
    while (n--) *p++ = 0.;
}

/* copie un vecteur */

void vcopy_f(float *f, float *g, int n)
{
    float *new, *old;
    new = g;
    old = f;
    while (n--) *new++ = *old++;
}

void vcopy(double *f, double *g, int n)
{
    double *new, *old;
    new = g;
    old = f;
    while (n--) *new++ = *old++;
}

/* vŽrifie si n est une puissance de 2 */

int powerof2p(long n)
{
    long exp = -1,nn = n;
    while (nn)
    {
        nn >>= 1;
        exp++;
    }
    return (n == (1 << exp));
}

/*
 #define RAND_MAX 32768.
 
 long state;
 short randi()
 {
 state = state * 210427233L + 1489352071L;
 return ((state >> 16) & 0x7fff);
 }
 */


/******************
 Interpolations
 ******************/

inline t_sample linear_interpol (t_sample *buf, float alpha) /*NEWTON*/
{
    int alpha0 = floor(alpha);
    t_sample beta0 = *(buf + alpha0);
    t_sample diff1 = buf[alpha0 + 1] - beta0;
    float npoly1 = alpha - alpha0;
    return(beta0 + diff1 * npoly1);
}

inline t_sample linear_interpol_f (t_word *buf, float alpha) /*NEWTON*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = buf[alpha0].w_float;
    t_float diff1 = buf[alpha0 + 1].w_float - beta0;
    float npoly1 = alpha - alpha0;
    return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_i (int16_t *buf, float alpha) /*NEWTON*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = *(buf + alpha0);
    t_float diff1 = buf[alpha0 + 1] - beta0;
    float npoly1 = alpha - alpha0;
    return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_i_big_endian (int16_t *buf, float alpha) /*NEWTON*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = (int16_t)  _af_byteswap_int16 (*(buf + alpha0));
    t_float diff1 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 1]) - beta0;
    float npoly1 = alpha - alpha0;
    return(beta0 + diff1 * npoly1);
}

inline t_float get_24bit_sample (t_float *buf,long index)
{
    unsigned char *bytes;
    bytes = (unsigned char *)buf;
    signed char a = bytes[index*3];
    unsigned char b = bytes[(index+1)*3];
    unsigned char c = bytes[(index+2)*3];
    t_float sample = ((65536 * a) + 256 * b + c) ;
    return sample;
}

inline t_float get_24bit_sample_b (unsigned char *bytes,long index)
{
    signed char a = bytes[index];
    unsigned char b = bytes[index+1];
    unsigned char c = bytes[index+2];
    t_float sample = (t_float)((65536 * a) + 256 * b + c) ;
    return sample;
}

inline t_float linear_interpol_i_24 ( unsigned char *bytes, float alpha) /*NEWTON*/
{
    // 24 bit = 3 octets
    long alpha0 = (long)alpha;
    t_float beta0 = get_24bit_sample_b(bytes,alpha0*3);
    t_float diff1 = get_24bit_sample_b(bytes,(alpha0+1) * 3) - beta0;
    float npoly1 = alpha - alpha0;
    return(beta0 + diff1 * npoly1);
}

inline t_float square_interpol_i_24 (unsigned char *bytes, float alpha) /*NEWTON - GREGORY*/
{
    long alpha0 = (long)alpha;
    t_float beta0 = get_24bit_sample_b(bytes,alpha0*3);
    t_float beta1 = get_24bit_sample_b(bytes,(alpha0+1) * 3);
    t_float beta2 = get_24bit_sample_b(bytes,(alpha0+2) * 3);
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float cubic_interpol_i_24 (unsigned char *bytes, const t_float alpha) /*NEWTON - GREGORY*/
{
    long alpha0 = (long)alpha - 1;   /* translation */
    t_float beta0 =  get_24bit_sample_b(bytes,alpha0*3);
    t_float beta1 = get_24bit_sample_b(bytes,(alpha0+1) * 3);
    t_float beta2 =get_24bit_sample_b(bytes,(alpha0+2) * 3);
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0 + 1;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
    t_float diff3 = get_24bit_sample_b(bytes,(alpha0+3) * 3) - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_sample square_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = floor(alpha);
    t_sample beta0 = buf[alpha0];
    t_sample beta1 = buf[alpha0 + 1];
    t_sample beta2 = buf[alpha0 + 2];
    t_sample diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0;
    t_sample diff2i = beta2 - beta1 ;
    t_sample diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_sample square_interpol_f (t_word *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = buf[alpha0].w_float;
    t_float beta1 = buf[alpha0 + 1].w_float;
    t_float beta2 = buf[alpha0 + 2].w_float;
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float square_interpol_i (int16_t *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = buf[alpha0];
    t_float beta1 = buf[alpha0 + 1];
    t_float beta2 = buf[alpha0 + 2];
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float square_interpol_i_big_endian (int16_t *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha;
    t_float beta0 = (int16_t)  _af_byteswap_int16 (buf[alpha0]);
    t_float beta1 = (int16_t)  _af_byteswap_int16 (buf[alpha0+1]);
    t_float beta2 = (int16_t)  _af_byteswap_int16 (buf[alpha0+2]);
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_sample cubic_interpol (t_sample *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = floor(alpha) - 1;   /* translation */
    t_sample beta0 = buf[alpha0];
    t_sample beta1 = buf[alpha0 + 1];
    t_sample beta2 = buf[alpha0 + 2];
    t_sample diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0 + 1;
    t_sample diff2i = beta2 - beta1 ;
    t_sample diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
    t_sample diff3 = buf[alpha0 + 3] - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_sample cubic_interpol_f (t_word *buf, float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha - 1;   /* translation */
    t_float beta0 = buf[alpha0].w_float;
    t_float beta1 = buf[alpha0 + 1].w_float;
    t_float beta2 = buf[alpha0 + 2].w_float;
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0 + 1;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
    t_float diff3 = buf[alpha0 + 3].w_float - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_float cubic_interpol_i_big_endian (const int16_t *buf, const t_float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha - 1;   /* translation */
    t_float beta0 = (int16_t)  _af_byteswap_int16 (buf[alpha0]);
    t_float beta1 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 1]);
    t_float beta2 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 2]);
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0 + 1;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
    t_float diff3 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 3]) - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_float cubic_interpol_i (const int16_t *buf, const t_float alpha) /*NEWTON - GREGORY*/
{
    int alpha0 = (int)alpha - 1;   /* translation */
    t_float beta0 = buf[alpha0];
    t_float beta1 = buf[alpha0 + 1];
    t_float beta2 = buf[alpha0 + 2];
    t_float diff1 = beta1 - beta0;
    float npoly1 = alpha - alpha0 + 1;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;  /* 2! */
    t_float diff3 = buf[alpha0 + 3] - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

/******************
 Interpolations for multi channel buffers
 ******************/

inline t_sample linear_interpol_n (t_sample *buf, long alpha0, float npoly1, long nchans) /*NEWTON*/
{
    t_sample beta0 = *(buf + alpha0);
    t_sample diff1 = buf[alpha0 + (1 * nchans)] - beta0;
    return(beta0 + diff1 * npoly1);
}

inline t_sample linear_interpol_n_24 (t_sample *buf, long alpha0, float npoly1, long nchans) /*NEWTON*/
{
    t_sample beta0 = get_24bit_sample(buf,alpha0);
    t_sample diff1 = get_24bit_sample(buf,alpha0 + 1 * nchans) - beta0;
    
    return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_n_f (t_float *buf, long alpha0, float npoly1, long nchans) /*NEWTON*/
{
    t_float beta0 = *(buf + alpha0);
    t_float diff1 = buf[alpha0 + (1 * nchans)] - beta0;
    return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_n_i_big_endian (int16_t *buf, long alpha0,float npoly1, long nchans) /*NEWTON*/
{

    t_float beta0 = (int16_t)  _af_byteswap_int16 (*(buf + alpha0));
    t_float diff1 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 1 * nchans]) - beta0;
     return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_n_f_24 (t_float *buf,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_float beta0 = get_24bit_sample(buf,alpha0);
    t_float beta1 = get_24bit_sample(buf,alpha0 + 1 * nchans);
    t_float diff1 = beta1 - beta0;
    return(beta0 + diff1 * npoly1);
}

inline t_float linear_interpol_n_i_24 (unsigned char *bytes,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_float beta0 = get_24bit_sample_b(bytes,alpha0 * 3);
    t_float beta1 = get_24bit_sample_b(bytes,(3 * (alpha0 + nchans)));
    t_float diff1 = beta1 - beta0;
    return(beta0 + diff1 * npoly1);
}

inline t_sample square_interpol_n (t_sample *buf, long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_sample beta0 = buf[alpha0];
    t_sample beta1 = buf[alpha0 + 1 * nchans];
    t_sample beta2 = buf[alpha0 + 2 * nchans];
    t_sample diff1 = beta1 - beta0;
    t_sample diff2i = beta2 - beta1 ;
    t_sample diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float square_interpol_n_f (t_float *buf, long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_float beta0 = buf[alpha0];
    t_float beta1 = buf[alpha0 + 1 * nchans];
    t_float beta2 = buf[alpha0 + 2 * nchans];
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float square_interpol_n_i_big_endian (int16_t *buf, long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_float beta0 = (int16_t)  _af_byteswap_int16(buf[alpha0]);
    t_float beta1 = (int16_t)  _af_byteswap_int16(buf[alpha0 + 1 * nchans]);
    t_float beta2 = (int16_t)  _af_byteswap_int16(buf[alpha0 + 2 * nchans]);
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 - 1.) ;
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_float square_interpol_n_i_24 (unsigned char *bytes,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    t_float beta0 = get_24bit_sample_b(bytes,alpha0 * 3);
    t_float beta1 = get_24bit_sample_b(bytes,(alpha0 + nchans) * 3);
    t_float beta2 = get_24bit_sample_b(bytes,(alpha0 + 2 * nchans) * 3);
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2);
}

inline t_sample cubic_interpol_n (t_sample *buf,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    alpha0 = alpha0 - 1 * nchans;   /* translation */
    t_sample beta0 = buf[alpha0];
    t_sample beta1 = buf[alpha0 + 1 * nchans];
    t_sample beta2 = buf[alpha0 + 2 * nchans];
    t_sample diff1 = beta1 - beta0;
    t_sample diff2i = beta2 - beta1 ;
    t_sample diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    t_sample diff3 = buf[alpha0 + 3 * nchans] - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_sample cubic_interpol_n_24 (t_sample *buf,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    alpha0 = alpha0 - 1 * nchans;   /* translation */
    t_sample beta0 = get_24bit_sample(buf,alpha0);
    t_sample beta1 = get_24bit_sample(buf,alpha0 + 1 * nchans);
    t_sample beta2 = get_24bit_sample(buf,alpha0 + 2 * nchans);
    t_sample diff1 = beta1 - beta0;
    t_sample diff2i = beta2 - beta1 ;
    t_sample diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    t_sample diff3 = buf[alpha0 + 3 * nchans] - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_float cubic_interpol_n_f (t_float *buf,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    alpha0 = alpha0 - 1 * nchans;   /* translation */
    t_float beta0 = buf[alpha0];
    t_float beta1 = buf[alpha0 + 1 * nchans];
    t_float beta2 = buf[alpha0 + 2 * nchans];
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    t_float diff3 = buf[alpha0 + 3 * nchans] - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_float cubic_interpol_n_i_big_endian (int16_t *buf,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    alpha0 = alpha0 - 1 * nchans;   /* translation */
    t_float beta0 = (int16_t)  _af_byteswap_int16 (buf[alpha0]);
    t_float beta1 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 1 * nchans]);
    t_float beta2 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 2 * nchans]);
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    t_float diff3 = (int16_t)  _af_byteswap_int16 (buf[alpha0 + 3 * nchans]) - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

inline t_float cubic_interpol_n_i_24 (unsigned char *bytes,long alpha0, float npoly1, long nchans) /*NEWTON - GREGORY*/
{
    alpha0 = alpha0 - 1 * nchans;   /* translation */
    t_float beta0 = get_24bit_sample_b(bytes,alpha0 * 3);
    t_float beta1 = get_24bit_sample_b(bytes,(alpha0 + nchans) * 3);
    t_float beta2 = get_24bit_sample_b(bytes,(alpha0 + 2 * nchans) * 3);
    t_float diff1 = beta1 - beta0;
    t_float diff2i = beta2 - beta1 ;
    t_float diff2 = diff2i - diff1;
    float npoly2 = 0.5 * npoly1 * (npoly1 -  1 ) ;  /* 2! */
    t_float diff3 = get_24bit_sample_b(bytes,(alpha0 + 3 * nchans) * 3) - beta2 - diff2i - diff2;
    float npoly3 = npoly2 * (npoly1 - 2.) / 6.;    /* 3! */
    return(beta0 + diff1 * npoly1 + diff2 * npoly2 + diff3 * npoly3);
}

/*********************
 Fenetres
 **********************/
void triangular_window(t_float *wind, int size)
{
    int i,half = size / 2.;
    for(i = 0;i < half ;i++)
        *wind++ = (float)i / (float) half;
    for(i = half;i < size ;i++)
        *wind++ = (float)(- 2. * i / size + 2.);
}

void rectangular_window(t_float *wind, int size)
{
    int i;
    for(i = 0;i < 50 ;i++)
        *wind++ = (float)i/50.;
    for(i = 50;i < size - 50 ;i++)
        *wind++ = 1.;
    for(i = size - 50;i < size ;i++)
        *wind++ = (float)((float)-i + (float)size) / 50.;
}

void cresc_window(t_float *wind, int size)
{
    int i;
    float iscale;
    for (i = 0; i < size; i++)
    {iscale=(float)i*50./(float)size - 40.;/* from -40 to +10 dB */
        *wind++ = pow(10.,(iscale/20.))/3.17;}
    
}
void decresc_window(t_float *wind, int size)
{
    int i;
    float iscale;
    for (i = 0; i < size; i++){
        iscale=(float)i*-50./(float)size + 10.;/* from +10 tO -40  DB */
        *wind++ = pow(10.,(iscale/20.))/3.17;
    }
}
void hamming_window(t_float *wind, int size)
{
    int i,half = size / 2.;
    float pi =  4 * atan(1.);
    for(i = 0;i < size ;i++)
        *wind++ = (cos(pi + pi * ((float)i / (float)half)) + 1.) / 2.;
}

void hamming32_window(t_float *wind, int size)
{
    int i,half = size / 2.;
    float pi =  4 * atan(1.);
    
    for (i = 0; i < size; i++)
        if (i < half / 32)
            *wind++ =(cos(pi + pi * (i / ((float)half / 32.))) + 1.) / 2.;
        else if (i >= size - (half / 32))
            *wind++ =(cos(pi + pi * (((float)i - (float)half) / ((float)half / 32.))) + 1.) / 2.;
        else
            *wind++ =1.;
}

#if 0
long  ilog2(long n)
{
    long ret = -1;
    while (n) {
        n >>= 1;
        ret++;
    }
    return (ret);
}
#endif
