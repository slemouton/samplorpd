/*USEFUL MACROS*/
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#define interpol(x1, x2, i) ((x1) * (1.0 - i) + (x2) * i)
#define cent_to_linear(x) (exp(0.00057762265047 * (x)))
#define midi_to_freq(x) (440.0 * exp(0.057762265047 * ((x) - 69.0)))
#define dB_to_linear(x) (exp(0.11512925465 * (x)))

static inline uint16_t _af_byteswap_int16 (uint16_t x)
{
    return (x >> 8) | (x << 8);
}

/**************************************************/
float slm1_get_value(t_atom *a);
t_symbol *slm1_get_symbol(t_atom  *a);
void vzero(double *f,int n);
void vcopy(double *f, double *g, int n);
void vzero_f(float *f,int n);
void vcopy_f(float *f, float *g, int n);
int powerof2p(long n);

t_float get_24bit_sample (t_float *buf,long index);
t_float get_24bit_sample_b (unsigned char *bytes,long index);

t_sample linear_interpol (t_sample *buf, float alpha);
t_sample linear_interpol_f (t_word *buf, float alpha);
t_float linear_interpol_i (t_int16 *buf, float alpha);
t_float linear_interpol_i_big_endian (t_int16 *buf, float alpha);
t_float square_interpol_i_big_endian (t_int16 *buf, float alpha);
t_float linear_interpol_i_24 (unsigned char *bytes, float alpha);
t_float square_interpol_i_24 (unsigned char *bytes, float alpha);
t_float cubic_interpol_i_24 (unsigned char *bytes, float alpha);
t_sample square_interpol (t_sample *buf, float alpha);
t_sample square_interpol_f (t_word *buf, float alpha);
t_float square_interpol_i (t_int16 *buf, float alpha);
t_float square_interpol_i_big_endian (t_int16 *buf, float alpha);
t_sample cubic_interpol (t_sample *buf, float alpha);
t_sample cubic_interpol_f (t_word *buf, float alpha);
t_float cubic_interpol_i (const t_int16 *buf, const t_float alpha);
t_float cubic_interpol_i_big_endian (const t_int16 *buf, const t_float alpha);
t_sample linear_interpol_n (t_sample *buf, long alpha0, float npoly1, long nchans);
t_sample linear_interpol_n_24 (t_sample *buf, long alpha0, float npoly1, long nchans);
t_float linear_interpol_n_i_big_endian (int16_t *buf, long alpha,float npoly1, long nchans);
t_float linear_interpol_n_f_24 (t_float *buf, long alpha0, float npoly1, long nchans);
t_float linear_interpol_n_i_24 (unsigned char *buf,long alpha0, float npoly1, long nchans);

t_float cubic_interpol_n_i_big_endian (int16_t *buf, long alpha,float npoly1, long nchans);
t_float square_interpol_n_i_big_endian (int16_t *buf, long alpha,float npoly1, long nchans);
t_float linear_interpol_n_f (t_float *buf, long alpha0, float npoly1, long nchans);
t_sample square_interpol_n (t_sample *buf, long alpha0, float npoly1, long nchans);
t_float square_interpol_n_f (t_float *buf, long alpha0, float npoly1, long nchans);
t_sample cubic_interpol_n (t_sample *buf, long alpha0, float npoly1, long nchans);
t_sample cubic_interpol_n_24 (t_sample *buf, long alpha0, float npoly1, long nchans);
t_float cubic_interpol_n_f (t_float *buf, long alpha0, float npoly1, long nchans);

t_float square_interpol_n_i_24 (unsigned char *buf, long alpha0, float npoly1, long nchans);
t_float cubic_interpol_n_i_24 (unsigned char *buf, long alpha0, float npoly1, long nchans);

void triangular_window (t_float *wind, int size);
void rectangular_window (t_float *wind, int size);
void cresc_window (t_float *wind, int size);
void decresc_window (t_float *wind, int size);
void hamming_window (t_float *wind, int size);
void hamming32_window (t_float *wind, int size);
//long ilog2(long n);


