#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"
#include "slm1.h"

#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif
#define VERSION "samplor~: version v0.0.82 = MAX version 3.64"

/* ------------------------ samplorpd~ ----------------------------- */

static t_class *samplorpd_class;

/* ------------------------ DSP METHODS ----------------------------- */

/*
 * samplor_run_one runs one voice and returns 0 if the voice can be freed and 1
 * if it is still needed, TOUT EST LA !
 */
int samplor_run_one64(t_samplor_entry *x, t_sample **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_sample *out1 = out[0];
    t_sample *out2 = out[1];
    t_sample *out3 = out[2];
    t_float sample = 0;
    t_float *tab,w_f_index,f;
    t_float amp_scale;
    int index,index2, w_index, frames, nc;
    const t_float *window = windows + 512 * x->win;
    t_float xfade_amp = 0;
    unsigned int in_xfadeflag = 0;
    register int samplecount = 0;
    register long m;
    
    //internal buffer :
    t_samplorbuffer *mybuf = x->samplor_buf;
    if (mybuf)
    {
        if ((tab = mybuf->f_samples))
        {frames = mybuf->b_frames;
            nc = mybuf->b_nchans;}
        else goto zero;
    }
    else if (garray_getfloatwords(x->buf, &frames, &tab))
    {
        nc = 1;
    }
    else goto zero;
#if DELAY_ACTIVE
    if (x->start > 0)
    {
        m =  min(n, x->start);
        n -= m;
        out += m;
        x->start -= m;
    }
#endif
    if (x->count > 0)
    {
        if(x->loop_flag == LOOP)
            m=n;
        else
        {
            m = min(n, x->count);
            x->count -= m;
        }
        while (m--)
        {
            /*BOUCLE*/
#if 1
            samplor_compute_loop(x,m, loop_xfade, &in_xfadeflag,&xfade_amp);
#else
            if((x->loop_flag == LOOP)|| (x->loop_flag == FINISHING_LOOP))
            {
                if(x->fposition >= x->loop_end)  /* on boucle */
                {
                    x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
                    x->loop_end = x->loop_end_d;
                    x->loop_dur = x->loop_dur_d;
                    x->fposition -= x->loop_dur;
                    
                    if(x->loop_flag == FINISHING_LOOP)
                    {   if(x->count < m)
                        x->loop_flag = 0;
                    }
                    else
                    {
                        in_xfadeflag=0;
                        x->count += 1 + x->loop_dur / x->increment; /* I don't really know why the 1 ????*/
                    }
                }
                else if(loop_xfade && (x->fposition > (x->loop_end - loop_xfade))) /* dans la zone de cross-fade */
                {
                    in_xfadeflag=1;
                    xfade_amp = ((x->fposition - x->loop_end) / loop_xfade) + 1.;
                }
                else
                    in_xfadeflag=0;
            }
            else if((x->loop_flag == ALLER_RETOUR_LOOP) || (x->loop_flag == IN_ALLER_RETOUR_LOOP))
            {
                if(x->fposition >= x->loop_end)
                {
                    x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
                    x->loop_dur = x->loop_end - x->loop_beg;
                    x->count += x->loop_dur / x->increment;
                    x->increment = - x->increment;
                    x->loop_flag = IN_ALLER_RETOUR_LOOP;
                }
                else if((x->fposition <= x->loop_beg) && (x->loop_flag == IN_ALLER_RETOUR_LOOP))
                {
                    /* eventually, modify loop points */
                    x->loop_end = x->loop_end_d;
                    x->loop_dur = x->loop_end - x->loop_beg;
                    x->increment = - x->increment;
                    x->count += x->loop_dur / x->increment;
                }
            }
#endif
            f = x->fposition;
            index = (int)f;
            index2 = (int)x->fposition2;
            x->fposition += x->increment * modwheel ;
            x->fposition2 += x->increment * modwheel;
            
            //////////////////
            /* 2.ECHANTILLON*/
            if(!interpol)
            {
                if (index < 0)
                    index = 0;
                else if (index >= frames)
                {
                    index = frames - 1;
                }
                index = index * nc ;
                sample = tab[index];
                if(in_xfadeflag)
                {/* do the loop cross fade !*/
                    sample *= 1. - xfade_amp;
                    sample +=  xfade_amp * tab[index - x->loop_dur];
                }
            }
            else
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames)
                    f = frames - 1;
                if (nc > 1)
                {
                    f *=  nc ;
                    //t_buffer_info info;
                    //  buffer_getinfo(x->buf_obj,&info);
                    post ("interleaved stereo buffer, please use stereo (-2) mode");
                    //  object_error ((t_object *)x,"interleaved stereo buffer, please use stereo (-2) mode");
                    goto zero;
                }
                if(!in_xfadeflag)
                {
                    if(interpol == 1)
                        sample = linear_interpol_f (tab,f);
                    else if(interpol == 2)
                        sample = square_interpol_f (tab,f);
                    else     // if(interpol == 3)
                        sample = cubic_interpol_f (tab,f);
                }
                else
                {              /*LOOP CROSSFADE : */
                    if (interpol == 2)
                    {
                        sample = (1. - xfade_amp) * square_interpol_f (tab,f);
                        sample +=  xfade_amp * square_interpol_f (tab,f- x->loop_dur);
                    }
                    else if(interpol == 1)
                    {
                        sample = (1. - xfade_amp) * linear_interpol_f (tab,f);
                        sample +=  xfade_amp * linear_interpol_f (tab,f- x->loop_dur);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sample = (1. - xfade_amp) * cubic_interpol_f (tab,f);
                        sample +=  xfade_amp * cubic_interpol_f (tab,f- x->loop_dur);
                    }
                    else if(interpol == 4)
                    {  /* constant power crossfade for uncorrelated loops */
                        sample = sqrt(1. - xfade_amp)  * cubic_interpol_f (tab,f);
                        sample +=  sqrt(xfade_amp) * cubic_interpol_f (tab,f- x->loop_dur);
                    }
                }
            }
            
            /* LOOPING :*/
            if((x->loop_flag == LOOP)||(x->loop_flag == FINISHING_LOOP))
                index = index2;
            
            /* AMPLITUDE ENVELOPE :*/
            if((x->win) && !x->loop_flag) //pas de window si on boucle !
            {
                w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
                w_index = min(w_index,WIND_SIZE);  // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                sample *= x->amplitude * *(window + w_index);
            }
            
            /* attack-release stuff */
            
            if (index < x->attack)
            {
                w_f_index = (float)(index - x->begin) * x->attack_ratio;
            }
            else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
            {
                w_f_index = (float)( x->end - index) * x->release_ratio;
                //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                w_f_index = powf(w_f_index , x->release_curve);
            }
            else if (index < x->decay)
            {
                w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
            }
            else w_f_index = x->sustain;
            
            w_f_index = max (w_f_index,0.); // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
            
            sample *=  w_f_index;
            sample *= x->amplitude;
            
            if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                {
                    x->fade_out_end = (index2 + x->fade_out_time);
                }
                amp_scale = (float)(x->fade_out_end - index2) / x->fade_out_time;
                amp_scale = powf(amp_scale,x->release_curve);
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (amp_scale < 0)
                    goto zero;
                sample *= amp_scale;
            }
            
            /*PAN & AUX :*/
            if(num_outputs > 3)
            {
#ifdef MULTIPAN
                out[x->chan2][samplecount] += sample * x->pan;
                out[x->chan][samplecount++] += sample * (1. - x->pan);
#else
                out[x->chan][samplecount++] += sample;
#endif
            }
            else if (num_outputs > 1)
            {
                *out2++ += sample * x->pan;
                if (num_outputs > 2)
                    *out3++ += sample * x->rev;
                *out1++ += (1. - x->pan) * sample;
            }
            else
                *out1++ += sample;
        }
#if THREAD_SAFE
        buffer_unlocksamples(x->buf_obj);
#endif
        return 1;
    }
    else
    {
    zero:
#if THREAD_SAFE
        buffer_unlocksamples(x->buf_obj);
#endif
        return 0;
    }
}

//for internal integer buffers
int samplor_run_one64_int(t_samplor_entry *x, t_sample **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_sample *out1 = out[0];
    t_sample *out2 = out[1];
    t_sample *out3 = out[2];
    t_float sample = 0;
    int16_t *tab;
    t_float w_f_index,f;
    t_float amp_scale;
    int index,index2, w_index, frames, nc;
    const t_float *window = windows + 512 * x->win;
    t_float xfade_amp = 0;
    unsigned int in_xfadeflag = 0;
    register int samplecount = 0;
    register long m;
    float one_over_maxvalue;
    
    //internal buffer :
    t_samplorbuffer *mybuf = x->samplor_buf;
    if (mybuf)
    {
        if ((tab = mybuf->b_samples))
        {frames = mybuf->b_frames;
            nc = mybuf->b_nchans;
            one_over_maxvalue = x->samplor_buf->one_over_b_maxvalue;
        }
        else goto zero;
    }
    else goto zero;
#if DELAY_ACTIVE
    if (x->start > 0)
    {
        m =  min(n, x->start);
        n -= m;
        out += m;
        x->start -= m;
    }
#endif
    if (x->count > 0)
    {
        if(x->loop_flag == LOOP)
            m=n;
        else
        {
            m = min(n, x->count);
            x->count -= m;
        }
        while (m--)
        {
            samplor_compute_loop(x,m, loop_xfade, &in_xfadeflag,&xfade_amp);
            
            f = x->fposition;
            index = (int)f;
            index2 = (int)x->fposition2;
            x->fposition += x->increment * modwheel ;
            x->fposition2 += x->increment * modwheel;
            /*ECHANTILLON*/
            if(!interpol)
            {
                if (index < 0)
                    index = 0;
                else if (index >= frames)
                {
                    index = frames - 1;
                }
                if (nc > 1)
                    index = index * nc ;
                sample = (t_float) tab[index];
                if(in_xfadeflag)
                {/* do the loop cross fade !*/
                    sample *= 1. - xfade_amp;
                    sample +=  xfade_amp * tab[index - x->loop_dur];
                }
            }
            else
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames)
                    f = frames - 1;
                if (nc > 1)
                    f *=  nc ;
                if(!in_xfadeflag)
                {
                    if(interpol == 1)
                        sample = linear_interpol_i (tab,f);
                    else if(interpol == 2)
                        sample = square_interpol_i (tab,f);
                    else     // if(interpol == 3)
                        sample = cubic_interpol_i (tab,f);
                }
                else
                {              /*LOOP CROSSFADE : */
                    if (interpol == 2)
                    {
                        sample = (1. - xfade_amp) * square_interpol_i (tab,f);
                        sample +=  xfade_amp * square_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 1)
                    {
                        sample = (1. - xfade_amp) * linear_interpol_i (tab,f);
                        sample +=  xfade_amp * linear_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sample = (1. - xfade_amp) * cubic_interpol_i (tab,f);
                        sample +=  xfade_amp * cubic_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 4)
                    {  /* constant power crossfade for uncorrelated loops */
                        sample = sqrt(1. - xfade_amp)  * cubic_interpol_i (tab,f);
                        sample +=  sqrt(xfade_amp) * cubic_interpol_i (tab,f- x->loop_dur);
                    }
                }
            }
            sample *= one_over_maxvalue;
            
            /* LOOPING :*/
            if((x->loop_flag == LOOP)||(x->loop_flag == FINISHING_LOOP))
                index = index2;
            
            /* AMPLITUDE ENVELOPE :*/
            if((x->win) && !x->loop_flag) //pas de window si on boucle !
            {
                w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
                w_index = min(w_index,WIND_SIZE);  // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                sample *= x->amplitude * *(window + w_index);
#if WINAR
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if (index > x->release)
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                    /* new in version 2.95 !!!!*/
                    w_f_index = pow(w_f_index , x->release_curve);
                }
                else w_f_index = 1.;
                sample *= w_f_index;
#endif
            }
            else if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                {
                    x->fade_out_end = (index2 + x->fade_out_time);
                }
                amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = powf(amp_scale,x->release_curve);
                amp_scale = MAX(0.,amp_scale);
                sample *= amp_scale;
            }
            {
                /* attack-release stuff */
                index = min (index,x->dur); // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                    w_f_index = powf(w_f_index , x->release_curve);
                }
                else if (index < x->decay)
                {
                    w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
                }
                else w_f_index = x->sustain;
                
                sample *=  w_f_index;
            }
            sample *= x->amplitude;
            /*PAN & AUX :*/
            if(num_outputs > 3)
            {
#ifdef MULTIPAN
                out[x->chan2][samplecount] += sample * x->pan;
                out[x->chan][samplecount++] += sample * (1. - x->pan);
#else
                out[x->chan][samplecount++] += sample;
#endif
            }
            else if (num_outputs > 1)
            {
                *out2++ += sample * x->pan;
                if (num_outputs > 2)
                    *out3++ += sample * x->rev;
                *out1++ += (1. - x->pan) * sample;
            }
            else
                *out1++ += sample;
        }
#if THREAD_SAFE
        buffer_unlocksamples(x->buf_obj);
#endif
        return 1;
    }
    else
    {
    zero:
#if THREAD_SAFE
        buffer_unlocksamples(x->buf_obj);
#endif
        return 0;
    }
}
// for directtodisk

int samplor_run_one64_mmap_int(t_samplor_entry *x, t_sample **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
#if 0
    t_sample *out1 = out[0];
    t_sample *out2 = out[1];
    t_sample *out3 = out[2];
    t_float sample = 0;
    int16_t *tab;
    unsigned char *bytes;
    t_float w_f_index,f;
    t_float amp_scale;
    int index,index2, w_index, frames, nc;
    const t_float *window = windows + 512 * x->win;
    t_float xfade_amp = 0;
    unsigned int in_xfadeflag = 0;
    register int samplecount = 0;
    register long m;
    float one_over_maxvalue;

    if (x->mmap_buf)  /* direct to disk */
    {
        tab = x->mmap_buf->addr + x->mmap_buf->offset - x->mmap_buf->pa_offset;
        bytes = tab;
        frames = x->mmap_buf->b_frames;
        nc = x->mmap_buf->b_nchans;
        one_over_maxvalue = x->mmap_buf->one_over_b_maxvalue;
    }
    else goto zero;
    
//#if DELAY_ACTIVE
    if (x->start > 0)
    {
        m =  min(n, x->start);
        n -= m;
        out += m;
        x->start -= m;
    }
//#endif
    if (x->count > 0)
    {
        if(x->loop_flag == LOOP)
            m=n;
        else
        {
            m = min(n, x->count);
            x->count -= m;
        }
        while (m--)
        {
            samplor_compute_loop(x,m, loop_xfade, &in_xfadeflag,&xfade_amp);
            
            f = x->fposition;
            index = (long)f;
            index2 = (long)x->fposition2;
            x->fposition += x->increment * modwheel ;
            x->fposition2 += x->increment * modwheel;
            
            /*2 Sample access*/
            if(!interpol)
            {
                if (index < 0)
                    index = 0;
                else if (index >= frames)
                {
                    index = frames - 1;
                }
                if (nc > 1)
                    index = index * nc ;
                if(x->mmap_buf->b_samplewidth == 16)
                {
                    if(x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        sample = (t_float) tab[index];
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sample *= 1. - xfade_amp;
                            sample +=  (t_float) xfade_amp * tab[index - x->loop_dur];
                        }
                    }
                    else
                    {
                        sample = (int16_t) _af_byteswap_int16 (tab[index]);
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sample *= 1. - xfade_amp;
                            sample +=  (t_float) xfade_amp * _af_byteswap_int16 (tab[index - x->loop_dur]);
                        }
                    }
                }
                else if(x->mmap_buf->b_samplewidth == 24)
                {
                    long i = index * 3;
                    signed char a = bytes[i];
                    unsigned char b = bytes[i+1];
                    unsigned char c = bytes[i+2];
                    if (x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        double d = (65536 * a) + 256 * b + c ;
                        sample = (t_float) d;
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sample *= 1. - xfade_amp;
                            sample +=  (t_float) xfade_amp * tab[index - x->loop_dur];
                        }
                    }
                    else
                    {
                        int d = (65536 * a) + 256 * b + c ;
                        sample = (t_float) d;
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sample *= 1. - xfade_amp;
                            sample +=  (t_float) xfade_amp * _af_byteswap_int16 (tab[index - x->loop_dur]);
                        }
                    }
                }
            }
            else // interpolation
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames)
                    f = frames - 1;
                if (nc > 1)
                {
                    f *=  nc ;
                    // t_buffer_info info;
                    // buffer_getinfo(x->buf_obj,&info);
                    post ("interleaved stereo buffer, please use stereo (-2) mode");
                    //  object_error ((t_object *)x,"interleaved stereo buffer, please use stereo (-2) mode");
                    goto zero;
                }
                if((x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN) && x->mmap_buf->b_samplewidth == 16)
                {if(!in_xfadeflag)
                {
                    if(interpol == 1)
                        sample = linear_interpol_i (tab,f);
                    else if(interpol == 2)
                        sample = square_interpol_i (tab,f);
                    else     // if(interpol == 3)
                        sample = cubic_interpol_i (tab,f);
                }
                else
                {              /*LOOP CROSSFADE : */
                    if (interpol == 2)
                    {
                        sample = (1. - xfade_amp) * square_interpol_i (tab,f);
                        sample +=  xfade_amp * square_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 1)
                    {
                        sample = (1. - xfade_amp) * linear_interpol_i (tab,f);
                        sample +=  xfade_amp * linear_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sample = (1. - xfade_amp) * cubic_interpol_i (tab,f);
                        sample +=  xfade_amp * cubic_interpol_i (tab,f- x->loop_dur);
                    }
                    else if(interpol == 4)
                    {  /* constant power crossfade for uncorrelated loops */
                        sample = sqrt(1. - xfade_amp)  * cubic_interpol_i (tab,f);
                        sample +=  sqrt(xfade_amp) * cubic_interpol_i (tab,f- x->loop_dur);
                    }
                }
                }
                else if((x->mmap_buf->byteOrder == AF_BYTEORDER_BIGENDIAN) && x->mmap_buf->b_samplewidth == 16)
                {if(!in_xfadeflag)
                {
                    if(interpol == 1)
                        sample = linear_interpol_i_big_endian (tab,f);
                    else if(interpol == 2)
                        sample = square_interpol_i_big_endian (tab,f);
                    else     // if(interpol == 3)
                        sample = cubic_interpol_i_big_endian (tab,f);
                }
                else
                {              /*LOOP CROSSFADE : */
                    if (interpol == 2)
                    {
                        sample = (1. - xfade_amp) * square_interpol_i_big_endian (tab,f);
                        sample +=  xfade_amp * square_interpol_i_big_endian (tab,f- x->loop_dur);
                    }
                    else if(interpol == 1)
                    {
                        sample = (1. - xfade_amp) * linear_interpol_i_big_endian (tab,f);
                        sample +=  xfade_amp * linear_interpol_i_big_endian (tab,f- x->loop_dur);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sample = (1. - xfade_amp) * cubic_interpol_i_big_endian (tab,f);
                        sample +=  xfade_amp * cubic_interpol_i_big_endian (tab,f- x->loop_dur);
                    }
                    else if(interpol == 4)
                    {  /* constant power crossfade for uncorrelated loops */
                        sample = sqrt(1. - xfade_amp)  * cubic_interpol_i_big_endian (tab,f);
                        sample +=  sqrt(xfade_amp) * cubic_interpol_i_big_endian (tab,f- x->loop_dur);
                    }
                }
                }
                else if(x->mmap_buf->b_samplewidth == 24)
                {
                    if(!in_xfadeflag)
                    {
                        if(interpol == 1)
                            sample = linear_interpol_i_24 (bytes,f);
                        else if(interpol == 2)
                            sample = square_interpol_i_24 (bytes,f);
                        else     // if(interpol == 3)
                            sample = cubic_interpol_i_24 (bytes,f);
                    }
                    else
                    {              /*LOOP CROSSFADE : */
                        if (interpol == 2)
                        {
                            sample = (1. - xfade_amp) * square_interpol_i_24 (bytes,f);
                            sample +=  xfade_amp * square_interpol_i_24 (bytes,f- x->loop_dur);
                        }
                        else if(interpol == 1)
                        {
                            sample = (1. - xfade_amp) * linear_interpol_i_24 (bytes,f);
                            sample +=  xfade_amp * linear_interpol_i_24 (bytes,f- x->loop_dur);
                        }
                        else if(interpol == 3)
                        {  /* optimisation possible (un seul appel a cubic_interpol()) */
                            sample = (1. - xfade_amp) * cubic_interpol_i_24 (bytes,f);
                            sample +=  xfade_amp * cubic_interpol_i_24 (bytes,f- x->loop_dur);
                        }
                        else if(interpol == 4)
                        {  /* constant power crossfade for uncorrelated loops */
                            sample = sqrt(1. - xfade_amp)  * linear_interpol_i_24 (bytes,f);
                            sample +=  sqrt(xfade_amp) * linear_interpol_i_24(bytes,f- x->loop_dur);
                        }
                    }
                }
            }
            sample *= one_over_maxvalue;
            
            /* LOOPING :*/
            if((x->loop_flag == LOOP)||(x->loop_flag == FINISHING_LOOP))
                index = index2;
            
            /* AMPLITUDE ENVELOPE :*/
            if((x->win) && !x->loop_flag) //pas de window si on boucle !
            {
                w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
                w_index = min(w_index,WIND_SIZE);  // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                sample *= x->amplitude * *(window + w_index);
//#if WINAR
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if (index > x->release)
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                    /* new in version 2.95 !!!!*/
                    w_f_index = pow(w_f_index , x->release_curve);
                }
                else w_f_index = 1.;
                sample *= w_f_index;
//#endif
            }
            else if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                {
                    x->fade_out_end = (index2 + x->fade_out_time);
                }
                amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = powf(amp_scale,x->release_curve);
                amp_scale = MAX(0.,amp_scale);
                sample *= amp_scale;
            }
            {
                /* attack-release stuff */
                index = min (index,x->dur); // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                    w_f_index = powf(w_f_index , x->release_curve);
                }
                else if (index < x->decay)
                {
                    w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
                }
                else w_f_index = x->sustain;
                
                sample *=  w_f_index;
            }
            sample *= x->amplitude;
            /*PAN & AUX :*/
            if(num_outputs > 3)
            {
                out[x->chan][samplecount++] += sample;
            }
            else if (num_outputs > 1)
            {
                *out2++ += sample * x->pan;
                if (num_outputs > 2)
                    *out3++ += sample * x->rev;
                *out1++ += (1. - x->pan) * sample;
            }
            else
                *out1++ += sample;
        }
//#if THREAD_SAFE
//        buffer_unlocksamples(x->buf_obj);
//#endif
        return 1;
    }
    else
    {
    zero:
//#if THREAD_SAFE
//        buffer_unlocksamples(x->buf_obj);
//#endif

    }
    #endif
        return 0;
    
}

/*
 * samplor_run_one runs one voice and returns 0 if the voice can be freed and 1
 * if it is still needed, TOUT EST LA !
 */
int samplor_run_one_stereo64(t_samplor_entry *x, t_double **out, long n, t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_double *out1 = out[0];
    t_double *out2 = out[1];
    t_float sampleL = 0,sampleR = 0;
    float *tab,w_f_index;
    float amp_scale;
    double f,f2;
    long index, index2, w_index, frames, nc;
    t_float *window = windows + 512 * x->win;
    float xfade_amp = 0.;
    long in_xfadeflag = 0;
    long samplecount = 0;
    long m;
#if 0
    tab = buffer_locksamples(x->buf_obj);
    if (!tab)
        goto zero;
    frames = buffer_getframecount(x->buf_obj);
    nc = buffer_getchannelcount(x->buf_obj);
/*#if DELAY_ACTIVE
    if (x->start > 0)
    {
        m =  min(n, x->start);
        n -= m;
        out += m;
        x->start -= m;
    }
#endif*/
    if (x->count > 0)
    {
        if(x->loop_flag == LOOP)
            m=n;
        else
        {
            m = min(n, x->count);
            x->count -= m;
        }
        while (m--)
        {
            samplor_compute_loop(x,m, loop_xfade, &in_xfadeflag,&xfade_amp);
            
            f = x->fposition;
            index = (long)f;
            index2 = (long)x->fposition2;
            x->fposition += x->increment * modwheel;
            x->fposition2 += x->increment * modwheel;
            /*ECHANTILLON*/
            if (index < 0)
                index = 0;
            else if (index > frames)
                index = frames - 1;
            index2 = index * nc;
            if(!interpol)
            {
                sampleL = tab[index2];
                sampleR = tab[index2 + (nc - 1)];
                if(in_xfadeflag)
                {/* do the loop cross fade !*/
                    sampleL *= 1. - xfade_amp;
                    sampleR *= 1. - xfade_amp;
                    sampleL +=  xfade_amp * tab[index2 - (nc * x->loop_dur)];
                    sampleR +=  xfade_amp * tab[index2 + (nc - 1) - (nc * x->loop_dur)];
                }
            }
            else
            {
                if (f < 0)
                    f = 0;
                else if (f > frames)
                    f = frames - 3;
                f2 = f - floor(f);
                f = (double) index2;
                f += f2;
                if(!in_xfadeflag)
                {
                    if(interpol == 2)
                    {
                        sampleL = square_interpol_n_f (tab,index2,f2,nc);
                        sampleR = square_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                    }
                    else if(interpol == 1)
                    {
                        sampleL = linear_interpol_n_f (tab,index2,f2,nc);
                        sampleR = linear_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                    }
                    
                    else if(interpol == 3)
                    {
                        sampleL = cubic_interpol_n_f (tab,index2,f2,nc);
                        sampleR = cubic_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                    }
                }
                else
                /*LOOP CROSSFADE : */
                {
                    if(interpol == 1)
                    {
                        sampleL = (1. - xfade_amp) * linear_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * linear_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * linear_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * linear_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur), f2,nc);
                    }
                    else if(interpol == 2)
                    {
                        sampleL = (1. - xfade_amp) * square_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * square_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * square_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * square_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur),f2,nc);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sampleL = (1. - xfade_amp) * cubic_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * cubic_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * cubic_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * cubic_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur),f2,nc);
                    }
                }
            }
            /* AMPLITUDE ENVELOPE :*/
            if((x->loop_flag == LOOP)||(x->loop_flag == FINISHING_LOOP))
                index = index2;
            if((x->win) && !x->loop_flag ) //pas de window si on boucle !
            {
                w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
                w_index = min(w_index,WIND_SIZE);  // pour eviter d'aller apres la fin
                sampleL *= x->amplitude * *(window + w_index);
                sampleR *= x->amplitude * *(window + w_index);
/*
 #if WINAR
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if (index > x->release)
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    //    w_f_index = (float)((long)x->fposition - x->end)* x->release_ratio;
                    w_f_index = pow(w_f_index , x->release_curve);
                    
                }
                else w_f_index = 1.;
                sampleL *= w_f_index;
                sampleR *= w_f_index;
#endif*/
            }
            {
                /* attack-release stuff */
                
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    w_f_index = powf(w_f_index , x->release_curve);
                }
                else if (index < x->decay)
                {
                    w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
                }
                else
                    w_f_index = x->sustain;
                w_f_index = max (w_f_index,0.); // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                
                sampleL *= x->amplitude * w_f_index;
                sampleR *= x->amplitude * w_f_index;
            }
            //         if(x->fade_out_time) //for clean voice_stealing
            if (0)
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index2 + x->fade_out_time);
                
                //           amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = (float)(x->fade_out_end - index2) / x->fade_out_time;
                amp_scale = powf(amp_scale,x->release_curve);
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (amp_scale < 0)
                    goto zero;
                
                sampleL *= amp_scale;
                sampleR *= amp_scale;
            }
            
            if(x->fade_out_time) //for cleaner voice_stealing
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index2 + x->fade_out_time);
                
                //           amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = (float)(x->fade_out_counter) / x->fade_out_time;
                amp_scale = powf(amp_scale,x->release_curve);
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (x->fade_out_counter-- < 0)
                    goto zero;
                
                sampleL *= amp_scale;
                sampleR *= amp_scale;
            }
            
            if(num_outputs > 3)
            {
                out[x->chan][samplecount++] += sampleL;
            }
            else
            {
                *out2++ += sampleR * x->pan;
                *out1++ += (1. - x->pan) * sampleL;
            }
        }
        return 1;
    }
    else
        zero:
#endif
        return 0;
}

int samplor_run_one_stereo64_mmap(t_samplor_entry *x, t_double **out, long n, t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_double *out1 = out[0];
    t_double *out2 = out[1];
    t_float sampleL = 0,sampleR = 0;
    int16_t *tab;
    t_float w_f_index;
    unsigned char *bytes;
    t_float amp_scale;
    t_float f,f2;
    long index, index2, w_index, frames, nc;
    t_float *window = windows + 512 * x->win;
    t_float xfade_amp = 0.;
    unsigned int in_xfadeflag = 0;
    register int samplecount = 0;
    register long m;
    float one_over_maxvalue;
#if 0
    if (x->mmap_buf)  /* direct to disk */
    {
        tab = x->mmap_buf->addr + x->mmap_buf->offset - x->mmap_buf->pa_offset;
        bytes = tab;
        frames = x->mmap_buf->b_frames;
        nc = x->mmap_buf->b_nchans;
        one_over_maxvalue = x->mmap_buf->one_over_b_maxvalue;
    }
    else goto zero;
/*
#if DELAY_ACTIVE
    if (x->start > 0)
    {
        m =  min(n, x->start);
        n -= m;
        out += m;
        x->start -= m;
    }
#endif
 */
    if (x->count > 0)
    {
        if(x->loop_flag == LOOP)
            m=n;
        else
        {
            m = min(n, x->count);
            x->count -= m;
        }
        while (m--)
        {
            /* 1. sample index*/
            samplor_compute_loop(x,m, loop_xfade, &in_xfadeflag,&xfade_amp);
            
            f = x->fposition;
            index = (long)f;
            index2 = (long)x->fposition2;
            x->fposition += x->increment * modwheel;
            x->fposition2 += x->increment * modwheel;
            
            /* 2.sample access ECHANTILLON*/
            if (index < 0)
                index = 0;
            else if (index > frames)
                index = frames - 1;
            index2 = index * nc;
            if(!interpol)
            {
                if(x->mmap_buf->b_samplewidth == 16)
                {
                    if(x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        sampleL = (t_float) tab[index2];
                        sampleR = (t_float) tab[index2 + (nc - 1)];
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sampleL *= 1. - xfade_amp;
                            sampleR *= 1. - xfade_amp;
                            sampleL +=  xfade_amp * tab[index2 - (nc * x->loop_dur)];
                            sampleR +=  xfade_amp * tab[index2 + (nc - 1) - (nc * x->loop_dur)];
                        }
                    }
                    else
                    {
                        sampleL = (int16_t) _af_byteswap_int16 (tab[index2]);
                        sampleR = (int16_t) _af_byteswap_int16 (tab[index2 + (nc - 1)]);
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sampleL *= 1. - xfade_amp;
                            sampleR *= 1. - xfade_amp;
                            sampleL +=  (t_float) xfade_amp * _af_byteswap_int16 (tab[index2 - (nc * x->loop_dur)]);
                            sampleR +=  (t_float) xfade_amp * _af_byteswap_int16 (tab[index2 + (nc - 1) - (nc * x->loop_dur)]);
                        }
                    }
                }
                else if(x->mmap_buf->b_samplewidth == 24)
                {
                    long i = index2 * 3;
                    long i2 = (index2 + (nc - 1)) * 3;
                    signed char a = bytes[i];
                    signed char a2 = bytes[i2];
                    unsigned char b = bytes[i+1];
                    unsigned char b2 = bytes[i2+1];
                    unsigned char c = bytes[i+2];
                    unsigned char c2 = bytes[i2+2];
                    if (x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        double d = (65536 * a) + 256 * b + c ;
                        sampleL = (t_float) d;
                        d = (65536 * a2) + 256 * b2 + c2 ;
                        sampleR = (t_float) d;
                        if(in_xfadeflag)
                        {/* do the loop cross fade !*/
                            sampleL *= 1. - xfade_amp;
                            //  sampleL +=  (t_float) xfade_amp * tab[index - x->loop_dur];
                            sampleR *= 1. - xfade_amp;
                            // sampleR +=  (t_float) xfade_amp * tab[index - x->loop_dur];
                        }
                    }
                    else
                    {
                        int d = (65536 * a) + 256 * b + c ;
                        sampleL = (t_float) d;
                        d = (65536 * a2) + 256 * b2 + c2 ;
                        sampleR = (t_float) d;
                        {/* do the loop cross fade !*/
                            sampleL *= 1. - xfade_amp;
                            // sampleL +=  (t_float) xfade_amp * _af_byteswap_int16 (tab[index - x->loop_dur]);
                        }
                    }
                }
            }
            else  // we are in interpolation mode
            {
                if (f < 0)
                    f = 0;
                else if (f > frames)
                    f = frames - 3;
                f2 = f - floor(f);
                f = (double) index2;
                f += f2;
                if(!in_xfadeflag)
                {
                    if(x->mmap_buf->b_samplewidth == 16 && x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        if(interpol == 2)
                        {
                            sampleL = square_interpol_n_f (tab,index2,f2,nc);
                            sampleR = square_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        }
                        else if(interpol == 1)
                        {
                            sampleL = linear_interpol_n_f (tab,index2,f2,nc);
                            sampleR = linear_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        }
                        else // if(interpol == 3)
                        {
                            sampleL = cubic_interpol_n_f (tab,index2,f2,nc);
                            sampleR = cubic_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        }
                    }
                    else if(x->mmap_buf->b_samplewidth == 16 && x->mmap_buf->byteOrder == AF_BYTEORDER_BIGENDIAN)
                    {
                        if(interpol == 2)
                        {
                            sampleL = square_interpol_n_i_big_endian (tab,index2,f2,nc);
                            sampleR = square_interpol_n_i_big_endian (tab,index2 + (nc - 1),f2,nc);
                        }
                        else if(interpol == 1)
                        {
                            sampleL = linear_interpol_n_i_big_endian (tab,index2,f2,nc);
                            sampleR = linear_interpol_n_i_big_endian (tab,index2 + (nc - 1),f2,nc);
                        }
                        else // if(interpol == 3)
                        {
                            sampleL = cubic_interpol_n_i_big_endian (tab,index2,f2,nc);
                            sampleR = cubic_interpol_n_i_big_endian (tab,index2 + (nc - 1),f2,nc);
                        }
                    }
                    
                    else if(x->mmap_buf->b_samplewidth == 24)
                    {
                        if(interpol == 2)
                        {
                            sampleL = square_interpol_n_i_24 (bytes,index2,f2,nc);
                            sampleR = square_interpol_n_i_24 (bytes,index2 + (nc - 1),f2,nc);
                        }
                        else if(interpol == 1)
                        {
                            sampleL = linear_interpol_n_i_24 (bytes,index2,f2,nc);
                            sampleR = linear_interpol_n_i_24 (bytes,index2 + (nc - 1),f2,nc);
                        }
                        else // if(interpol == 3)
                        {
                            sampleL = cubic_interpol_n_i_24 (bytes,index2,f2,nc);
                            sampleR = cubic_interpol_n_i_24 (bytes,index2 + (nc - 1),f2,nc);
                        }
                    }
                }
                else  /* we are in a LOOP CROSSFADE : */
                {
                    if(interpol == 1)
                    {
                        sampleL = (1. - xfade_amp) * linear_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * linear_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * linear_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * linear_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur), f2,nc);
                    }
                    else if(interpol == 2)
                    {
                        sampleL = (1. - xfade_amp) * square_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * square_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * square_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * square_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur),f2,nc);
                    }
                    else if(interpol == 3)
                    {  /* optimisation possible (un seul appel a cubic_interpol()) */
                        sampleL = (1. - xfade_amp) * cubic_interpol_n_f (tab,index2,f2,nc);
                        sampleL +=  xfade_amp * cubic_interpol_n_f (tab,index2 - (nc * x->loop_dur),f2,nc);
                        sampleR = (1. - xfade_amp) * cubic_interpol_n_f (tab,index2 + (nc - 1),f2,nc);
                        sampleR +=  xfade_amp * cubic_interpol_n_f (tab,index2 + (nc - 1) - (nc * x->loop_dur),f2,nc);
                    }
                }
            }
            sampleL *= one_over_maxvalue;
            sampleR *= one_over_maxvalue;
            /* AMPLITUDE ENVELOPE :*/
            if((x->loop_flag == LOOP)||(x->loop_flag == FINISHING_LOOP))
                index = index2;
            if((x->win) && !x->loop_flag ) //pas de window si on boucle !
            {
                w_index = WIND_SIZE * ((long) x->fposition - x->begin) / x->dur;
                w_index = min(w_index,WIND_SIZE);  // pour eviter d'aller apres la fin
                sampleL *= x->amplitude * *(window + w_index);
                sampleR *= x->amplitude * *(window + w_index);
/*#if WINAR
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if (index > x->release)
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    w_f_index = pow(w_f_index , x->release_curve);
                    
                }
                else w_f_index = 1.;
                sampleL *= w_f_index;
                sampleR *= w_f_index;
#endif*/
            }
            {
                /* attack-release stuff */
                
                if (index < x->attack)
                {
                    w_f_index = (float)(index - x->begin) * x->attack_ratio;
                }
                else if ((index > x->release) && (x->loop_flag != LOOP)&& (!x->fade_out_time))
                {
                    w_f_index = (float)( x->end - index) * x->release_ratio;
                    w_f_index = powf(w_f_index , x->release_curve);
                }
                else if (index < x->decay)
                {
                    w_f_index = 1. + (float)(index - x->attack ) * x->decay_ratio;
                }
                else
                    w_f_index = x->sustain;
                w_f_index = max (w_f_index,0.); // pour eviter d'aller apres la fin de l'enveloppe si modwheel est utilisé
                
                sampleL *= x->amplitude * w_f_index;
                sampleR *= x->amplitude * w_f_index;
            }
            //         if(x->fade_out_time) //for clean voice_stealing
            if (0)
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index2 + x->fade_out_time);
                
                //           amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = (float)(x->fade_out_end - index2) / x->fade_out_time;
                amp_scale = powf(amp_scale,x->release_curve);
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (amp_scale < 0)
                    goto zero;
                
                sampleL *= amp_scale;
                sampleR *= amp_scale;
            }
            if(x->fade_out_time) //for cleaner voice_stealing
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index2 + x->fade_out_time);
                
                //           amp_scale = (float)(x->fade_out_end - index2) * x->release_ratio2;
                amp_scale = (float)(x->fade_out_counter) / x->fade_out_time;
                amp_scale = powf(amp_scale,x->release_curve);
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (x->fade_out_counter-- < 0)
                    goto zero;
                
                sampleL *= amp_scale;
                sampleR *= amp_scale;
            }
            
            if(num_outputs > 3)
            {
                out[x->chan][samplecount++] += sampleL;
            }
            else
            {
                *out2++ += sampleR * x->pan;
                *out1++ += (1. - x->pan) * sampleL;
            }
        }
        return 1;
    }
    else
        zero:
#endif
        return 0;
}

int samplor_run_one_lite64(t_samplor_entry *x, t_sample **out, int n,long interpol)
{
    t_sample *out1 = out[0];
    t_float sample = 0.;
    t_float *tab_up = 0,w_f_index,amp_scale;
    t_word *tab;
    t_float f;
    int index, frames, nc;

    //internal buffer :
    if (x->samplor_buf)
    {
        tab = x->samplor_buf->f_samples ;
        tab_up = x->samplor_buf->f_upsamples ;
        frames = x->samplor_buf->b_frames;
        nc = x->samplor_buf->b_nchans;
    }
    else if (garray_getfloatwords(x->buf, &frames, &tab))
    {
        nc = 1;
    }
    else goto zero;
    
    if (x->count > 0)
    {
        long m = min(n, x->count);
        
        x->count -= m;
        while (m--)
        {
            f = x->fposition;
            index = (int)f;
            x->fposition += x->increment ;
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames)
                    f = frames - 1;
                if (nc > 1)
                    f *=  nc ;
                {
                    if(interpol == 1)
                        sample = linear_interpol_f(tab,f);
                    else if(interpol == 2)
                        sample = linear_interpol_f(tab,f);
                    else if(interpol == 3)
                        sample = cubic_interpol_f (tab,f);
                    else if(interpol == 5)
                        sample = tab_up[(long)(f  *  UPSAMPLING)];
                    else
                        sample = tab[index].w_float;
                }
            }
            /* attack-release stuff */
            if (index < x->attack)
            {
                w_f_index = (float)(index - x->begin) * x->attack_ratio;
            }
            else if (index > x->release)
            {
                w_f_index = (float)( x->end - index)* x->release_ratio;
            }
            else if (index < x->decay)
            {
                w_f_index = 1. + (float)(index - x->attack) * x->decay_ratio;
            }
            else
                w_f_index = x->sustain;
            sample *= x->amplitude * w_f_index;
            if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index + x->fade_out_time);
                
                amp_scale = (float)(x->fade_out_end - index) / (float) x->fade_out_time;
                
                //amp_scale = MAX(0.,amp_scale);
                // or (to instantly free the voice )
                if (amp_scale < 0)
                    goto zero;
                sample *= amp_scale;
            }
            *out1++ += sample;
        }
        return (1);
    }
    else
        zero:
        return (0);
}

int samplor_run_one_lite64_int(t_samplor_entry *x, t_sample **out, int n,long interpol)
{
    t_sample *out1 = out[0];
    t_float sample = 0.;
    float w_f_index;
    int16_t *tab;
    double f;
    int index, frames, nc;
    float one_over_maxvalue;
    
    if (x->samplor_buf)  /* est-ce necessaire (l'info est déja dans la structure "entry", non ? */
    {
        tab = x->samplor_buf->b_samples ;
        frames = x->samplor_buf->b_frames;
        nc = x->samplor_buf->b_nchans;
        one_over_maxvalue = x->samplor_buf->one_over_b_maxvalue;
    }
    else if (garray_getfloatwords(x->buf, &frames, &tab))
    {
        nc = 1;
    }
    else goto zero;
    
    if (x->count > 0)
    {
        long m = min(n, x->count);
        x->count -= m;
        while (m--)
        {
            f = x->fposition;
            index = (long)f;
            x->fposition += x->increment ;
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames) // OPTIMISATIONS : ce test est evitable !!!
                    f = frames - 1;
                if (nc > 1)
                    f *=  nc ;
                if(interpol == 1)
                    sample = linear_interpol_i(tab,f);
                else if(interpol == 2)
                    sample = square_interpol_i(tab,f);
                else if(interpol == 3)
                    sample = cubic_interpol_i(tab,f);
                else
                    sample = (t_float) tab[index];
                sample *= one_over_maxvalue;
            }
            /* attack-release stuff */
            if (index < x->attack)
            {
                w_f_index = (float)(index - x->begin) * x->attack_ratio;
            }
            else if (index > x->release)
            {
                w_f_index = (float)( x->end - index)* x->release_ratio;
            }
            else if (index < x->decay)
            {
                w_f_index = 1. + (float)(index - x->attack) * x->decay_ratio;
            }
            else
                w_f_index = x->sustain;
            sample *= x->amplitude * w_f_index;
            if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index + x->fade_out_time);
                sample *= (float)(x->fade_out_end - index) / (float) x->fade_out_time;
            }
            *out1++ += sample;
        }
        return (1);
    }
    else
        zero:
        return (0);
}

int samplor_run_one_lite64_mmap_int(t_samplor_entry *x, t_sample **out, int n,long interpol)
{
    t_sample *out1 = out[0];
    t_float sample = 0.;
    float w_f_index;
    int16_t *tab;
    double f;
    long index, frames, nc;
    float one_over_maxvalue;
#if 0
    if (x->mmap_buf)  /* direct to disk */
    {
        tab = x->mmap_buf->addr + x->mmap_buf->offset - x->mmap_buf->pa_offset;
        frames = x->mmap_buf->b_frames;
        nc = x->mmap_buf->b_nchans;
        one_over_maxvalue = x->mmap_buf->one_over_b_maxvalue;
    }
    else goto zero;
    
    if (x->count > 0)
    {
        long m = min(n, x->count);
        if (x->mmap_buf )
            if (((x->fposition + m + interpol)  > (x->mmap_buf->index_end)) &&
                (x->fposition < x->mmap_buf->index_start))// reload the memory map
                if(samplor_do_mmap(x->mmap_buf,(off_t) x->fposition,(size_t) x->mmap_buf->length))
                    goto zero;
        
        x->count -= m;
        while (m--)
        {
            f = x->fposition;
            index = (long)f;
            x->fposition += x->increment ;
            {
                if (f < 0)
                    f = 0;
                else if (f >= frames) // OPTIMISATIONS : ce test est evitable !!!
                    f = frames - 1;
                if (nc > 1)
                    f *=  nc ;
                if(x->mmap_buf->b_samplewidth == 16)
                {
                    if(x->mmap_buf->byteOrder == AF_BYTEORDER_LITTLEENDIAN)
                    {
                        if(interpol == 1)
                            sample = linear_interpol_i(tab,f);
                        else if(interpol == 2)
                            sample = square_interpol_i(tab,f);
                        else if(interpol == 3)
                            sample = cubic_interpol_i(tab,f);
                        else
                            sample =  (t_float) tab[index];
                        
                    }
                    else if(interpol == 1)
                        sample = linear_interpol_i_big_endian(tab,f);
                    else if(interpol == 2)
                        sample = square_interpol_i_big_endian(tab,f);
                    else if(interpol == 3)
                        sample = cubic_interpol_i_big_endian(tab,f);
                    else
                        sample = (int16_t) _af_byteswap_int16(tab[index]);
                    
                    sample *= one_over_maxvalue;
                }
                else goto zero;
            }
            /* attack-release stuff */
            if (index < x->attack)
            {
                w_f_index = (float)(index - x->begin) * x->attack_ratio;
            }
            else if (index > x->release)
            {
                w_f_index = (float)( x->end - index)* x->release_ratio;
            }
            else if (index < x->decay)
            {
                w_f_index = 1. + (float)(index - x->attack) * x->decay_ratio;
            }
            else
                w_f_index = x->sustain;
            sample *= x->amplitude * w_f_index;
            if(x->fade_out_time) //for clean voice_stealing
            {
                if(!x->fade_out_end)
                    x->fade_out_end = (index + x->fade_out_time);
                sample *= (float)(x->fade_out_end - index) / (float) x->fade_out_time;
            }
            *out1++ += sample;
        }
        return (1);
    }
    else
        zero:
#endif
        return (0);
}

/*
 * samplor_run_all runs all voices and removes the finished voices
 * from the used list and puts them into the free list
 */
void samplor_run_all64(t_samplor *x, t_sample **outs, long n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    t_float *windows = (t_float *)x->windows;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one64(curr, outs, n, windows, num_outputs, x->interpol,x->loop_xfade,x->modwheel))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}

void samplor_run_all64_mmap_int(t_samplor *x, t_sample **outs, int n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    t_float *windows = (t_float *)x->windows;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one64_mmap_int(curr, outs, n, windows, num_outputs, x->interpol,x->loop_xfade,x->modwheel))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}

void samplor_run_all64_int(t_samplor *x, t_sample **outs, int n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    t_float *windows = (t_float *)x->windows;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one64_int(curr, outs, n, windows, num_outputs, x->interpol,x->loop_xfade,x->modwheel))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}


void samplor_run_all_stereo64_mmap(t_samplor *x, t_sample **outs, long n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    t_float *windows = (t_float *)x->windows;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one_stereo64_mmap(curr, outs, n, windows, num_outputs, x->interpol,x->loop_xfade,x->modwheel))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}

void samplor_run_all_stereo64(t_samplor *x, t_sample **outs, long n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    t_float *windows = (t_float *)x->windows;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one_stereo64(curr, outs, n, windows, num_outputs, x->interpol,x->loop_xfade,x->modwheel))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}
void samplor_run_all_lite64_mmap_int(t_samplor *x, t_sample **outs, int n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one_lite64_mmap_int(curr, outs, n, x->interpol))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}
/* lite version : mono, no delay, no windows */
void samplor_run_all_lite64(t_samplor *x, t_sample **outs, int n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one_lite64(curr, outs, n, x->interpol))
        { /* next voice */
            prev = curr;
            curr = curr->next;
        }
        else
        {/* "removing one" */
            x->active_voices--;
            if(curr == prev)/*for the first time */
            {/* "in front" */
                x->list.used = curr->next;
                curr->next = x->list.free;
                x->list.free = curr;
                prev = curr = x->list.used;
            }
            else
            {
                curr = samplist_free_voice(&x->list,prev,curr);
            }
        }
    }
}

/* ------------------------ CTL METHODS ----------------------------- */

/*
 * samplor_init sets up the inputs and params structure with default values
 */

void samplor_init(t_samplor *x)
{
    x->inputs.offset = 0;
    x->inputs.dur = 0;
    x->inputs.attack = 2;
    x->inputs.decay = 2;
    x->inputs.sustain = 100;
    x->inputs.release = 2;
    x->inputs.susloopstart = 0;
    x->inputs.susloopend = 0;
    x->inputs.release_curve = 1.;
    x->inputs.transp = 1.;
    x->inputs.amp = 1.;
    x->inputs.pan = 0.5;
    x->inputs.rev = 1.;
    x->inputs.env = 1;        /* window type */
    x->inputs.buf_name = 0;
 //   x->inputs.buf = 0;
 //   x->inputs.buf_ref = 0;
    x->inputs.samplor_buf = 0;
    x->params.sr = DEFAULT_SRATE;
    x->params.sp = 1 / x->params.sr;
    samplor_windows(x);
    list_init(&x->waiting_notes);
    x->interpol = 1;        /* default : linear interpolation */
    x->voice_stealing = 0;    /* default : no voice stealing */
    x->loop_xfade = 0;    /* default : no loop crossfade */
    x->debug = 0;
    x->active_voices = 0;
    x->modwheel = 1.;
    x->n_sf = 1;
 //   x->buf_tab = (t_hashtab *)hashtab_new(0);//hashtable initialisation :
}

/*
 * make windows
 */

void samplor_windows(t_samplor *x)
{
    int size = WIND_SIZE;
  
    triangular_window(x->windows[1],size);
    rectangular_window(x->windows[2],size);
    cresc_window(x->windows[3],size);
    decresc_window(x->windows[4],size);
    hamming_window(x->windows[5],size);
    hamming32_window(x->windows[6],size);
}

void samplor_int(t_samplorpd *x,long d)
{
}

void samplor_debug(t_samplorpd *x,t_floatarg d)
{
    x->ctlp->debug = (long) d;
    post("debug %d",(long) d);
}

/*
 * don't use
 */
void
samplor_manual_init(t_samplorpd *x,long d)
{
    int i;
    post("init level %d",d);
    if (d == -2)
        samplor_init(x->ctlp);
    if (d>=4)
        x->ctlp = &(x->ctl);
    if(d>=3)
        samplor_init(x->ctlp);
    if(d>=2)
        samplor_maxvoices(x,12);
    if (d>=1)
    {
        x->time = 0;
        x->count = 0;
      }
    for (i=0;i<=x->num_outputs;i++)
        x->vectors[i] = NULL;
}

void samplor_count_active_voices(t_samplorpd *x,long d)
{
    int count;
    /*   samplist_display(&(x->ctlp->list)); */
    //post active voices to rightmost inlet
    if (d == 0)
        outlet_float(x->right_outlet,x->ctlp->active_voices);
    else if (d == 1){
        count = samplist_count(&(x->ctlp->list));
        outlet_float(x->right_outlet,count);}
    else if (d == 2){
        count = samplist_count(&(x->ctlp->list));
        post("active %d %d",count,x->ctlp->active_voices);
         outlet_float((t_object *)x->right_outlet,x->ctlp->active_voices);}
}

/*
 * samplor_maxvoices sets the maximum number of voices
*/
void samplor_maxvoices(t_samplorpd *x, long v)
{
   if (v > MAX_VOICES) {
        post("samplor~: maxvoices out of range (%d), set to %d", v, MAX_VOICES);
        v = MAX_VOICES;
    }
 if (v < 1)
        v = 1;
    // allocate space for the list
    x->ctlp->list.samplors = (t_samplor_entry *)realloc(x->ctlp->list.samplors,(2 + (2 * v)) * sizeof(t_samplor_entry));
    if (x->ctlp->list.samplors == NULL)
        error("problem allocating voices");
     x->ctlp->list.maxvoices = 2 + (2 * v);
    x->ctlp->polyphony = v;
   samplist_init(&(x->ctlp->list));
    
    x->ctlp->active_voices = 0;  
}

void samplor_interpol(t_samplorpd *x, t_floatarg f)
{
  
int interpol = (int)f;
    post("interpol %d",interpol);
    x->ctlp->interpol = interpol;
    if (x->ctlp->debug)
        switch(interpol)
    {
        case CLOSEST : post("no interpolation"); break;
        case LINEAR : post("interpolation lineaire"); break;
        case SQUARE : post("square interpolation"); break;
        case CUBIC : post("cubic interpolation"); break;
        case CUBIC2 : post("constant power crossfade for uncorrelated loops"); break;
        case UPSAMPLING2 : post("%d times oversampling ",UPSAMPLING); break;
        default : post("unknown interpolation mode\n");
    }
}

void samplor_start(t_samplorpd *x, float p)
{
    long start;
    t_samplor *ctlp = x->ctlp;
 //   t_buffer_obj *buf = buffer_ref_getobject(ctlp->inputs.buf_ref);
    t_garray *buf = ctlp->inputs.buf;
    
    start = p * 0.001 * ctlp->params.sr;
    if (ctlp->inputs.samplor_mmap)
    {
        if (0.001 * ctlp->inputs.samplor_mmap->b_sr * ctlp->inputs.offset < ctlp->inputs.samplor_mmap->b_frames)
        {
            ctlp->inputs.transp *= ctlp->inputs.samplor_mmap->b_sr / ctlp->params.sr;
            if (ctlp->active_voices < x->ctlp->polyphony)
            {
                samplist_insert(&(x->ctlp->list),start,ctlp->inputs);
                ctlp->active_voices++;
            }
            else if(ctlp->voice_stealing)
            {    /*clean voice stealing*/
                samplor_urgent_stop(ctlp,ctlp->voice_stealing);
                if (samplist_insert(&(x->ctlp->list),start,ctlp->inputs) != 0)
                {
                    ctlp->active_voices++;
                }
                else
                {
                    pd_error((t_object *) x,"too much voices : cancelling voice_stealing");
                    ctlp->voice_stealing = 0;
                }
            }
        }
    }
    else if (ctlp->inputs.samplor_buf)
    {
        if (ctlp->inputs.samplor_buf->b_sr/1000. * ctlp->inputs.offset < ctlp->inputs.samplor_buf->b_frames)
        {
            ctlp->inputs.transp *= ctlp->inputs.samplor_buf->b_sr / ctlp->params.sr;
            if (ctlp->active_voices < x->ctlp->polyphony)
            {
                samplist_insert(&(x->ctlp->list),start,ctlp->inputs);
                ctlp->active_voices++;
            }
            else if(ctlp->voice_stealing)
            {    /*clean voice stealing*/
                samplor_urgent_stop(ctlp,ctlp->voice_stealing);
                if (samplist_insert(&(x->ctlp->list),start,ctlp->inputs) != 0)
                {
                    ctlp->active_voices++;
                }
                else
                {
                    pd_error((t_object *) x,"too much voices : cancelling voice_stealing");
                    ctlp->voice_stealing = 0;
                }
            }
        }
    }
    else if (buf)
    {      /* n'alloue pas de voix si offset > duree du son */
        post ("offset %d framecount %d", ctlp->inputs.offset,garray_npoints(buf));
       // if (buffer_getmillisamplerate(buf) * ctlp->inputs.offset < buffer_getframecount(buf))
        {
            ctlp->inputs.transp *= 44100 / ctlp->params.sr;
            if (ctlp->active_voices < x->ctlp->polyphony)
            {
                samplist_insert(&(x->ctlp->list),start,ctlp->inputs);
                ctlp->active_voices++;
            }
            else if(ctlp->voice_stealing)
            {    /*clean voice stealing*/
                //samplor_stop_oldest(ctlp,ctlp->voice_stealing);//ajoute la pour 2.94 :
                samplor_urgent_stop(ctlp,ctlp->voice_stealing);
                if (samplist_insert(&(x->ctlp->list),start,ctlp->inputs) != 0)
                {
                    ctlp->active_voices++;
                    //samplor_urgent_stop(ctlp,ctlp->voice_stealing); //modifie pour 2.94 :
                }
                else
                {
                    pd_error((t_object *)x,"too much voices : cancelling voice_stealing");
                    ctlp->voice_stealing = 0;
                }
            }
        }
    }
}

/*
 * samplor_bang triggers a grain at next block begin
 */
void
samplor_bang(t_samplorpd *x)
{
    samplor_start(x, 0.);
}
/*
 * samplor_buf set the sound buffer
 */
void samplor_buf(t_samplorpd *x, int buf)
{
    char bufname[9] = "sample100";
    buf = min(buf,999);
    snprintf(bufname,sizeof(bufname),"sample%d",buf);
    x->ctlp->inputs.samplenumber = buf;
#if 1
    samplor_set(x, gensym(bufname));
#else
    t_atom av[1];
    atom_setsym(av, gensym(bufname));
    defer_low((t_object *)x,(method)samplor_deferredset,NULL,1,av);
#endif
}

/*
 * samplor_name set the sound buffer with the full name
 */
void samplor_bufname(t_samplorpd *x, char *name)
{
    char *bufname = "sample100";
    bufname = name;
    x->ctlp->inputs.samplenumber = -1;
    samplor_set(x,gensym(bufname));
}

/*
 * samplor_offset
 */
void samplor_offset(t_samplorpd *x, int offset)
{
    if (offset < 0)
        offset = 0;
    x->ctlp->inputs.offset = offset;
}

/*
 * samplor_dur set the duration
 */
void samplor_dur(t_samplorpd *x, double dur)
{
    x->ctlp->inputs.dur = dur;
}

/*
 * samplor_transp
 */
void samplor_transp(t_samplorpd *x, double transp)
{
    x->ctlp->inputs.transp = transp;
}

/*
 * samplor_amp
 */
void samplor_amp(t_samplorpd *x, double amp)
{
    x->ctlp->inputs.amp = amp;
}

/*
 * samplor_pan
 */
void samplor_pan(t_samplorpd *x, double pan)
{
    int chan;
    
    x->ctlp->inputs.pan = pan;
    if (x->num_outputs > 2)
    {
        chan = (int)pan;
        x->ctlp->inputs.pan -=  chan;
        if ((chan < 0) || (chan > (x->num_outputs -1)))
            chan = 0;
        x->ctlp->inputs.chan = chan;
        if(x->stereo_mode == 0)
            x->ctlp->inputs.chan2 = (chan + 1) % x->num_outputs;
        if(x->stereo_mode == 1)
            x->ctlp->inputs.chan2 = (chan + 1) % x->num_outputs;
    }
}

/*
 * samplor_rev
 */
void samplor_rev(t_samplorpd *x, double rev)
{
    int chan;
    
    x->ctlp->inputs.rev = rev; /* if multichannel this parameter can be used to control the output of the second channel of stereo buffers*/
    chan = (int)rev;
    x->ctlp->inputs.rev -=  chan;
    if ((chan < 0) || (chan > (x->num_outputs -1)))
        chan = 0;
    x->ctlp->inputs.chan3 = chan;
    if(x->stereo_mode == 1)
        x->ctlp->inputs.chan4 = (chan + 1) % x->num_outputs;
}

/*
 * samplor_win
 */
void samplor_win(t_samplorpd *x, int win)
{
    if ((win < 0) || (win > NUM_WINS))
        win = 0;
    x->ctlp->inputs.env = win;
    x->ctlp->inputs.attack = 2;
    x->ctlp->inputs.decay = 2;
    x->ctlp->inputs.sustain = 100;
    x->ctlp->inputs.release = 2;
}

int samplor_win_set(t_samplorpd *x , void *attr, long *ac, t_atom *av)
{
    if (ac && av) {
        t_int win = atom_getint(av);
        samplor_win(x,(int)win);
    }
    return 0;
}

int samplor_win_get(t_samplorpd *x, void *attr, long *ac, t_atom **av)
{
#if 0
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return 1;
        }
        atom_setlong(*av, x->ctlp->inputs.env);
    }
    #endif
    return 0;

}

/*
 * samplor_winar : window + atack and decay
 */
void samplor_winar(t_samplorpd *x, int win,int attack,int release)
{
    if ((win < 0) || (win > NUM_WINS))
        win = 0;
    x->ctlp->inputs.env = win;
    x->ctlp->inputs.attack = attack;
    x->ctlp->inputs.release = release;
}

/*
 * samplor_attack_release
 */
void
samplor_attack(t_samplorpd *x, int dur)
{  if (dur < 0)
    dur = 0;
    x->ctlp->inputs.attack = dur;
}
void
samplor_decay(t_samplorpd *x, int dur)
{  if (dur < 0)
    dur = 0;
    x->ctlp->inputs.decay = dur;
}
void
samplor_sustain(t_samplorpd *x, int val)
{  if (val < 0)
    val = 0;
    x->ctlp->inputs.sustain = val;
}
void
samplor_release(t_samplorpd *x, int dur)
{
    if (dur < 0)
        dur = 0;
    x->ctlp->inputs.release = dur;
}

void
samplor_adsr(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    x->ctlp->inputs.adsr_mode = 0;
    samplor_adsr_assign(x,ac,av);
}
void
samplor_adsr_ratio(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    x->ctlp->inputs.adsr_mode = 1;
    samplor_adsr_assign(x,ac,av);
}
void
samplor_adsr_assign(t_samplorpd *x,  short ac, t_atom *av)
{
    samplor_win(x,0);
    if (ac > 0) samplor_attack(x, atom_getfloat(av));
    if (ac > 1) samplor_decay(x, atom_getfloat(av + 1));
    if (ac > 2) samplor_sustain(x, atom_getfloat(av + 2));
    if (ac > 3) samplor_release(x, atom_getfloat(av + 3));
}

void samplor_curve(t_samplorpd *x, double curve)
{
    double c = max(0.01,curve);
    x->ctlp->inputs.release_curve = (t_samplor_real)c;
}

int samplor_curve_set(t_samplorpd *x , void *attr, long *ac, t_atom *av)
{
    if (ac && av) {
        t_atom_float curve = atom_getfloat(av);
        samplor_curve(x,curve);
    }
    return 0;
}

void samplor_curve_get(t_samplorpd *x, void *attr, long *ac, t_atom **av)
{
#if 0
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return 1;
        }
        atom_setfloat(*av, x->ctlp->inputs.release_curve);
    }
    return 0;
#endif

}

void samplor_compute_loop(t_samplor_entry *x,long m, long loop_xfade, unsigned int *in_xfadeflag,t_float *xfade_amp)
{
    if((x->loop_flag == LOOP)|| (x->loop_flag == FINISHING_LOOP))
    {
        if(x->fposition >= x->loop_end)  /* on boucle */
        {
            x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
            x->loop_end = x->loop_end_d;
            x->loop_dur = x->loop_dur_d;
            x->fposition -= x->loop_dur;
            
            if(x->loop_flag == FINISHING_LOOP)
            {   if(x->count < m)
                x->loop_flag = 0;
            }
            else
            {
                *in_xfadeflag = 0;
                x->count += 1 + x->loop_dur / x->increment; /* I don't really know why the 1 ????*/
            }
        }
        else if(loop_xfade && (x->fposition > (x->loop_end - loop_xfade))) /* dans la zone de cross-fade */
        {
            *in_xfadeflag = 1;
            *xfade_amp = ((x->fposition - x->loop_end) / loop_xfade) + 1.;
        }
        else
            in_xfadeflag=0;
    }
    else if((x->loop_flag == ALLER_RETOUR_LOOP) || (x->loop_flag == IN_ALLER_RETOUR_LOOP))
    {
        if(x->fposition >= x->loop_end)
        {
            x->loop_beg = x->loop_beg_d; /* eventually, modify loop points */
            x->loop_dur = x->loop_end - x->loop_beg;
            x->count += x->loop_dur / x->increment;
            x->increment = - x->increment;
            x->loop_flag = IN_ALLER_RETOUR_LOOP;
        }
        else if((x->fposition <= x->loop_beg) && (x->loop_flag == IN_ALLER_RETOUR_LOOP))
        {
            /* eventually, modify loop points */
            x->loop_end = x->loop_end_d;
            x->loop_dur = x->loop_end - x->loop_beg;
            x->increment = - x->increment;
            x->count += x->loop_dur / x->increment;
        }
    }
}
void samplor_loop_points(t_samplorpd *x, int start,int end)
{
    x->ctlp->inputs.susloopstart = start;
    x->ctlp->inputs.susloopend = end;
}

/*
 * utility to force the loop points in a buffer
 */
void samplor_get_buffer_loop(t_samplorpd *x, t_symbol *buffer_s)
{
#if 0
    t_buffer *b;
    t_atom myNumber[2];
    
    if ((b = (t_buffer *)(buffer_s->s_thing)) && ob_sym(b) == ps_buffer)
    {
        if (x->ctlp->debug) post("sustain loop %d %d", b->b_susloopstart, b->b_susloopend);        // in samples
        atom_setlong(&myNumber[0], b->b_susloopstart);
        atom_setlong(&myNumber[1], b->b_susloopend);
        outlet_anything(x->right_outlet, gensym("loop"), 2, myNumber);
    }
    else
    {
        error("samplor~: no buffer~ %s", buffer_s->s_name);
    }
#endif
}

void samplor_set_buffer_loop(t_samplorpd *x, t_symbol *buffer_s,long loopstart,long loopend)
{
#if 0
    t_buffer *b;
    
    if ((b = (t_buffer *)(buffer_s->s_thing)) && ob_sym(b) == ps_buffer)
    {
        long end = b->b_frames;
        if ((loopstart <= end) && (loopend <= end))
        {
            b->b_susloopstart = loopstart;
            b->b_susloopend = loopend;
        }
        else object_error((t_object *)x, "loop points are outside the %s buffer",b->b_name->s_name);
    }
    else object_error("samplor~: no buffer~ %s", buffer_s->s_name);
#endif
}

void samplor_modwheel(t_samplorpd *x, double transp)
{
    double transpo = max(0.1,transp);
    x->ctlp->modwheel = (t_samplor_real)transpo;
}

/*
 * to  stop  one sample (arret rapide)
 */
void samplor_stop2(t_samplorpd *x, long time)
{
    if (time > 0)
        samplor_urgent_stop(x->ctlp,time);
    else
        samplor_stop_oldest(x->ctlp,0 - time);
}

/*
 * to stop all samples (arret rapide)
 * stopall is deprecated, use stop message instead
 */
void samplor_stopall(t_samplorpd *x, long time)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    
    //   time *= x->ctlp->params.sr / 1000.;
    while (curr != LIST_END)
    {
        if(curr->loop_flag)
            curr->loop_flag = 0;
        {
            {
                curr->fade_out_time = time;
                //curr->fade_out_time = time / curr->increment;
                //curr->count = time;
            }
        }
        prev = curr;
        curr = curr->next;
    }
}

void samplor_urgent_stop(t_samplor *x,long time)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    if (curr)
    {
        time *= x->params.sr / 1000.;
        curr->loop_flag = 0;
        if (time / curr->increment < curr->count)
        { //modified in version 2.04
            curr->count = time / curr->increment ;
            curr->fade_out_time = time;
        }
    }
}

void samplor_stop_oldest(t_samplor *x,long time)
{
    t_samplor_entry *prev = x->list.at_end;
    t_samplor_entry *curr = prev;
    if (curr)
    {
        time *= x->params.sr / 1000.;
        curr->loop_flag = 0;
        if (time / curr->increment < curr->count)
        { //modified in version 2.94
            curr->count = time / curr->increment ;
            curr->fade_out_time = time;
        }
    }
}

/*
 * to stop a looped sample (sort de la boucle)
 */
void samplor_stop(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    int sample;
    
    switch (ac)
    {case 0 :
        {
            while (curr != LIST_END)
            {
                curr->loop_flag = 0;
                prev = curr;
                curr = curr->next;
            }
            break;
        }
        case 1:
        {
            sample = (int)atom_getfloat(av);
            while (curr != LIST_END)
            {
                if(curr->samplenumber == sample)
                    curr->loop_flag = 0;
                prev = curr;
                curr = curr->next;
            }
            break;
        }
        case 2:
        {samplor_stop_one_voice(x,(int)atom_getfloat(av),atom_getfloat(av + 1));
        }
    }
}

void samplor_stop_one_voice(t_samplorpd *x, int sample,float transp)
{
#if 0
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    long time;
    t_buffer_obj *buf = buffer_ref_getobject(x->ctlp->inputs.buf_ref);
    
    transp *= buffer_getsamplerate(buf) / x->ctlp->params.sr;
    while (curr != LIST_END)
    {
        if((curr->samplenumber == sample)&&((float)curr->increment==transp)&&(curr->loop_flag!=FINISHING_LOOP))
        {
            time = curr->end - curr->release;
            if(curr->loop_flag)
            {
                curr->loop_flag = FINISHING_LOOP;
                
                if (time)
                {
                    curr->fade_out_time = time * transp;
                    curr->fade_out_counter = curr->fade_out_time;
                }
                else // instant stop
                {
                    curr->fade_out_time = 1;
                    curr->fade_out_counter = 0;
                }
                break;
            }
            if (curr->count > time) //to avoid stopping notes already stopped but not yet completely gone ...
            {
                curr->fade_out_time = time * transp; /* correct fade out time */
                curr->fade_out_counter = curr->fade_out_time;
                curr->count = time;
                break;
            }
        }
        prev = curr;
        curr = curr->next;
    }
#endif
}

void samplor_stop_one_voice_str(t_samplorpd *x, t_symbol *buf_name,float transp)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    long time;

    while (curr != LIST_END)
    {
        if((!strcmp(buf_name->s_name,curr->buf_name->s_name))&&(((float)curr->increment==transp)||(transp==0))&&(curr->loop_flag!=FINISHING_LOOP))
        {
            time = curr->end - curr->release;
            if(curr->loop_flag)
            {
                //curr->loop_flag = FINISHING_LOOP;
                curr->loop_flag = 0;
                if (time)
                {
                    curr->fade_out_time = time * transp;
                    curr->fade_out_counter = curr->fade_out_time;
                }
                else // instant stop
                {
                    curr->fade_out_time = 1;
                    curr->fade_out_counter = 0;
                }
                if (x->loop_release)
                {
                    float start2 = 1000. * (float)curr->loop_end / 44100.;
                    if(x->ctlp->debug)
                        post ("buf %s amp %f loop_end %d %f %f",curr->buf_name->s_name,curr->amplitude,curr->loop_end,start2);
                    t_atom av[6];
                    SETFLOAT(av, 0);
                    SETSYMBOL(av+1, curr->buf_name); //buffer
                    SETFLOAT(av+2,start2 ); // start
                    SETFLOAT(av+3,0);               //dur
                    SETFLOAT(av+4,1 );       //transp
                    SETFLOAT(av+5,curr->amplitude);  //amp
                    samplor_list(x,curr->buf_name,6,av);
                }
                break;
            }
            
            if (curr->count > time) //to avoid stopping notes already stopped but not yet completely gone ...
            {
                curr->fade_out_time = time * curr->increment; /* correct fade out time */
                curr->fade_out_counter = curr->fade_out_time;
                curr->count = time;
            }
        }
        prev = curr;
        curr = curr->next;
    }
}

void samplor_stop_play(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    int del = 0,i;
    int susloopstart = 0;
    int susloopend = 0;
    int attack,decay,sustain,release;
    float transp = 1.,pan = 0.5,rev = 0.;
    int offset = 0;
    int dur = -1;
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    long time = 5;

    
    transp *= 44100. / x->ctlp->params.sr;
    if (ac > 1)
    {
        if (av->a_type == A_SYMBOL )
            samplor_bufname(x, av->a_w.w_symbol->s_name);
        else
            samplor_buf(x, atom_getfloat(av));
    }
    if (ac > 2)
        for (i = 1 ; i<ac;i++) // syntax parsing
        {
            if ((av + i)->a_type == A_SYMBOL )
            {
                if (!strcmp((av + i)->a_w.w_symbol->s_name,"loop"))
                {
                    susloopstart = atom_getfloat(av+i+1);
                    susloopend = atom_getfloat(av+i+2);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"adsr"))
                {
                    x->ctlp->inputs.adsr_mode = 0;
                    samplor_win(x,0);
                    if (ac > 0) attack =  atom_getfloat(av+i+1);
                    if (ac > 1) decay = atom_getfloat(av+i+2);
                    if (ac > 2) sustain = atom_getfloat(av+i+3);
                    if (ac > 3) release = atom_getfloat(av+i+4);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"delay"))
                {
                    del = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"offset"))
                {
                    offset = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"dur"))
                {
                    dur = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"transp"))
                {
                    transp = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"pan"))
                {
                    pan = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_symbol->s_name,"aux"))
                {
                    rev = atom_getfloat(av+i+1);
                }
            }
        }
    while (curr != LIST_END)
    {
        if((!strcmp(av->a_w.w_symbol->s_name,curr->buf_name->s_name))&&((float)curr->increment==transp)&&(susloopstart==(int)curr->loop_beg)&&(susloopend==(int)curr->loop_end)&&(curr->loop_flag!=FINISHING_LOOP))
        {
            time = curr->end - curr->release;
            if(curr->loop_flag)
            {
                curr->loop_flag = FINISHING_LOOP;
                if (time)
                {
                    curr->fade_out_time = time * transp;
                    curr->fade_out_counter = curr->fade_out_time;
                }
                else // instant stop
                {
                    curr->fade_out_time = 1;
                    curr->fade_out_counter = 0;
                }
                break;
            }
            if (curr->count > time) //to avoid stopping notes already stopped but not yet completely gone ...
            {
                curr->fade_out_time = time * curr->increment; /* correct fade out time */
                curr->fade_out_counter = curr->fade_out_time;
                curr->count = time;
                //    break;
            }
        }
        prev = curr;
        curr = curr->next;
    }
}

/*
 * to modify loop points
 */
void samplor_loop(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    int sample;
    float susloopstart = 0., susloopend = 0.;
    if (ac > 0) susloopstart = (int)atom_getfloat(av);
    if (ac > 1) susloopend = atom_getfloat(av + 1);
    if (ac > 2)
    {
        sample = (int)atom_getfloat(av + 2);
        while (curr != LIST_END)
        {
            if(curr->samplenumber == sample)
            {curr->loop_beg_d = susloopstart;
                curr->loop_end_d = susloopend;
                curr->loop_dur_d = susloopend - susloopstart;
                break;}
            prev = curr;
            curr = curr->next;
        }
    }
    else while (curr != LIST_END)
    {
        curr->loop_beg_d = susloopstart;
        curr->loop_end_d = susloopend;
        curr->loop_dur_d = susloopend - susloopstart;
        prev = curr;
        curr = curr->next;
    }
}

/*
 * samplor_list triggers one voice with : (delay, sample, offset, duration, ...)
 */
void samplor_list(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
    float amp;
    
    if (ac > 1)
    {
        if ((av + 1)->a_type == A_SYMBOL )
            samplor_bufname(x, atom_getsymbol(av + 1)->s_name);
        else
            samplor_buf(x, atom_getfloat(av + 1));
    }
    if (ac > 2) samplor_offset(x, atom_getfloat(av + 2));
    if (ac > 3) samplor_dur(x, atom_getfloat(av + 3));
    if (ac > 4) samplor_transp(x, atom_getfloat(av + 4));
    if (ac > 5)  // linear amplitude
    {
        if ((amp = atom_getfloat(av + 5)) > 0.)
        {
            samplor_amp(x,amp);
        }
        else     // noteoff
        {
            if (x->ctlp->inputs.samplenumber == -1)
                samplor_stop_one_voice_str(x,x->ctlp->inputs.buf_name,x->ctlp->inputs.transp);
            else
                samplor_stop_one_voice(x,x->ctlp->inputs.samplenumber,x->ctlp->inputs.transp);
            return;
        }
    }
    if (ac > 6) samplor_pan(x, atom_getfloat(av + 6));
    if (ac > 7) samplor_rev(x, atom_getfloat(av + 7));
    samplor_loop_points(x, 0, 0);
#if 0 //no defer in pureData ?
    if (x->thread_safe_mode == 1)
    {
        t_atom ava[1];
        atom_setfloat(ava, atom_getfloat(av));
        defer_low((t_object *)x,(t_method)samplor_start,NULL,1,ava);
    }
    else
#endif
        post ("starting %s %f",atom_getsymbol(av + 1)->s_name, atom_getfloat(av + 2));
        samplor_start(x, atom_getfloat(av));
}

/*
 * samplor_set set the sound buffer
 */
void samplor_set(t_samplorpd *x, t_symbol *s)
{

 //   t_buffer *b;
    t_garray *a=0;
    int npoints;
    t_float *src=(0);
    t_word *vec = 0;
    t_samplorbuffer *samplor_buffer;
  //  t_hashtab *buf_tab = x->ctlp->buf_tab;
    
    x->ctlp->inputs.buf_name = s;
    x->ctlp->inputs.buf = NULL;
    
    if (!(a = (t_garray *)pd_findbyclass(s, garray_class))){
        error("%s: no such array", s->s_name);
        x->ctlp->inputs.buf = NULL;
        return;
    } else
        if (!garray_getfloatwords(a, &npoints, &vec)){
            error("%s: bad template for tabread4", s->s_name);
            x->ctlp->inputs.buf = NULL;
            return;
        }
    x->ctlp->inputs.buf = a;
    for(int j = 0;j<8;j++)
    {post ("%d %f",j,vec[j].w_float);}
    
    x->ctlp->inputs.samplor_mmap = NULL;
    x->ctlp->inputs.samplor_buf = NULL;
#if 0
    if (!hashtab_lookup(buf_tab, s,(t_object **) &samplor_buffer))
    {
        if (x->dtd) // streaming mode
        {
            x->ctlp->inputs.samplor_mmap = samplor_buffer;
        }
        else     //internal buffer
        {
            x->ctlp->inputs.samplor_buf = samplor_buffer;
        }
    }
#endif
    if (!(x->ctlp->inputs.buf)&&!(x->ctlp->inputs.samplor_buf)&&!(x->ctlp->inputs.samplor_mmap))
        error("samplor~: no buf %s ", s->s_name);
}

void samplor_set_mac(t_samplorpd *x, t_symbol *s)
{
#if 0
    t_buffer *b;
    t_samplorbuffer *samplor_buffer;
    t_hashtab *buf_tab = x->ctlp->buf_tab;
    
    x->ctlp->inputs.buf_name = s;
    
    if(!(x->ctlp->inputs.buf_ref = buffer_ref_new((t_object*)x, s)))
        error("samplor~: no buffer~ %s", s->s_name);
    if (!(buffer_ref_exists(x->ctlp->inputs.buf_ref)))
        x->ctlp->inputs.buf_ref = NULL;
    if ((b = (t_buffer *)(s->s_thing)) && ob_sym(b) == ps_buffer)
        x->ctlp->inputs.buf = b;
    else
        x->ctlp->inputs.buf = NULL;
    
    x->ctlp->inputs.samplor_mmap = NULL;
    x->ctlp->inputs.samplor_buf = NULL;
    if (!hashtab_lookup(buf_tab, s,(t_object **) &samplor_buffer))
    {
        if (x->dtd) // streaming mode
        {
            x->ctlp->inputs.samplor_mmap = samplor_buffer;
        }
        else     //internal buffer
        {
            x->ctlp->inputs.samplor_buf = samplor_buffer;
        }
    }
    if (!(x->ctlp->inputs.buf_ref)&&!(x->ctlp->inputs.samplor_buf)&&!(x->ctlp->inputs.samplor_mmap))
        error("samplor~: no buf %s ", s->s_name);
#endif
}


/*
 * samplor_play triggers one voice with syntax buffer_name [optional_keyword values*]*
 * keyword list = loop (2 args) adsr (4 args) delay (1) offset (1) dur (1) transp (1) amp (1) pan (1) rev (1)
 * attention si le nombre d'argument n'est pas bon !
 */
void samplor_play(t_samplorpd *x, t_symbol *s, short ac, t_atom *av)
{
#if 0
    int del = 0,i;
    float amp = 1.;
    samplor_amp(x,amp);
    samplor_transp(x,1.);
    samplor_offset(x,0);
    samplor_dur(x,0);
    samplor_loop_points(x, 0, 0);
    if (ac > 1)
    {
        if (av->a_type == A_SYM )
            samplor_bufname(x, av->a_w.w_sym->s_name);
        else
            samplor_buf(x, atom_getfloat(av));
    }
    if (ac > 2)
        for (i = 1 ; i<ac;i++) // syntax parsing
        {
            if ((av + i)->a_type == A_SYM )
            {
                if (!strcmp((av + i)->a_w.w_sym->s_name,"loop"))
                {
                    samplor_loop_points(x,atom_getfloat(av+i+1),atom_getfloat(av+i+2));
                    samplor_dur(x,-1); // implicitly turn loop on
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"adsr"))
                {
                    x->ctlp->inputs.adsr_mode = 0;
                    samplor_win(x,0);
                    samplor_attack(x, atom_getfloat(av+i+1));
                    samplor_decay(x, atom_getfloat(av+i+2));
                    samplor_sustain(x, atom_getfloat(av+i+3));
                    samplor_release(x, atom_getfloat(av+i+4));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"delay"))
                {
                    del = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"offset"))
                {
                    samplor_offset(x,atom_getfloat(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"dur"))
                {
                    samplor_dur(x,atom_getfloat(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"transp"))
                {
                    samplor_transp(x,atom_getfloat(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"amp"))
                {
                    amp = atom_getfloat(av+i+1);
                    samplor_amp(x,amp);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"pan"))
                {
                    samplor_pan(x,atom_getfloat(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"aux"))
                {
                    samplor_rev(x,atom_getfloat(av+i+1));
                }
            }
        }
    if (amp == 0)
    { /* noteoff */
        if (x->ctlp->inputs.samplenumber == -1)
            samplor_stop_one_voice_str(x,x->ctlp->inputs.buf_name,x->ctlp->inputs.transp);
        else
            samplor_stop_one_voice(x,x->ctlp->inputs.samplenumber,x->ctlp->inputs.transp);
        return;
    }
    samplor_start(x, del);
#endif
}

static t_int *samplorpd_perform(t_int *w)
{
    t_float *in = (t_float *)(w[1]);
    t_float *out = (t_float *)(w[2]);
    int n = (int)(w[3]);
    while (n--)
    {
    float f = *(in++);
	*out++ = (f > 0 ? f : -f);
    }
    return (w+4);
}

static t_int *samplor_perform64N(t_int *w)
{
#if 0
    t_samplorpd *x = (t_samplorpd *)(w[1]):
    long i,j;
    long n = sys_getblksize();
    t_samplor *x_ctl = x->ctlp;
  //  void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    for (i=0;i<(x->num_outputs);i++)
        x->vectors64[i] = outs[i];
    
    i = n;
    while(i--)
    {
        for (j=0;j<(x->num_outputs);j++)
            x->vectors64[j][i] = 0.;
    }
    (fun_ptr)(x_ctl, x->vectors64, n, x->num_outputs);
#endif
    return (w+2);
}

static t_int *samplor_perform64_3(t_int *w)
{
    t_samplorpd *x = (t_samplorpd *)(w[1]);
    t_samplor *x_ctl = x->ctlp;
    t_sample *samplor_outs[3];
    long n = sys_getblksize();
    long i = n;
    t_sample *o1 = x->x_outvec[0];
    t_sample *o2 = x->x_outvec[1];
    t_sample *o3 = x->x_outvec[2];
    samplor_outs[0] =  x->x_outvec[0];
    samplor_outs[1] =  x->x_outvec[1];
    samplor_outs[2] =  x->x_outvec[2];
    
    while(i--)
    {
        *o1++ = 0.;
        *o2++ = 0.;
        *o3++ = 0.;
    }
    switch(x->buffer_mode)
    {
        case ARRAY: samplor_run_all64(x_ctl, samplor_outs, n, 1);break;
        case DTD: samplor_run_all_lite64_mmap_int(x_ctl, samplor_outs,n,1);break;
    }
    return(w+2);
}

static t_int *samplor_perform64_2(t_int *w)
{
    t_samplorpd *x = (t_samplorpd *)(w[1]);
    t_samplor *x_ctl = x->ctlp;
    t_sample *samplor_outs[2];
 //   void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    long n = sys_getblksize();
    long i = n;
    t_sample *o1 = x->x_outvec[0];
    t_sample *o2 = x->x_outvec[1];
    samplor_outs[0] = x->x_outvec[0];
    samplor_outs[1] = x->x_outvec[1];
    
    while(i--)
    {
        *o1++ = 0.;
        *o2++ = 0.;
    }
    //    samplor_run_all64(x_ctl, samplor_outs, n, 2);
    //(fun_ptr)(x_ctl, samplor_outs, n, 2);
    switch(x->buffer_mode)
    {
        case ARRAY: samplor_run_all64(x_ctl, samplor_outs, n, 1);break;
        case DTD: samplor_run_all_lite64_mmap_int(x_ctl, samplor_outs,n,1);break;
    }
    return(w+2);
}

static t_int *samplor_perform64_1(t_int *w)
{
    t_samplorpd *x = (t_samplorpd *)(w[1]);
    t_samplor *x_ctl = x->ctlp;
    t_sample *samplor_outs[1];

    //    void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    long n = sys_getblksize();
    long i = n;
    t_sample *o = x->x_outvec[0];
    samplor_outs[0] = x->x_outvec[0];
    while(i--)
        *o++ = 0.;
   
    switch(x->num_outputs)
    {
        case 1 :
            switch(x->buffer_mode)
        {
            case ARRAY: samplor_run_all64(x_ctl, samplor_outs, n, 1);break;
            case DTD: samplor_run_all_lite64_mmap_int(x_ctl, samplor_outs,n,1);break;
        }
            break;
        case 0 :
            switch(x->buffer_mode)
        {
            case ARRAY: samplor_run_all_lite64(x_ctl, samplor_outs, n, 1);break;
            case DTD: samplor_run_all_lite64_mmap_int(x_ctl, samplor_outs,n,1);break;
        }
            break;
    }
    return(w+2);
}


//------------------------------------------------------------------------------
// samplorpd_dsp - installs this object's dsp function in pd's callback list
//------------------------------------------------------------------------------
static void samplorpd_dsp(t_samplorpd *x, t_signal **sp)
{

    x->ctlp->params.sr = sys_getsr();
    x->ctlp->params.vs = sys_getblksize();
    //   for (i = 0; i < noutlets; i++)
    //      x->x_outvec[i] = sp[i]->s_vec;
    x->x_outvec[0] = sp[0]->s_vec;
    switch(x->num_outputs)
    {
        case 3: dsp_add(samplor_perform64_3,1,x);break;
        case 2: dsp_add(samplor_perform64_2,1,x);break;
        case 1:
        default: dsp_add(samplor_perform64_1,1,x);break;
    }
}

static void *samplorpd_new(t_symbol *s, int argc, t_atom *argv)
{
    long n,i;
    long numoutputs;
    long maxvoices = DEFAULT_MAXVOICES;
    t_samplorpd *x = NULL;
    
    x = (t_samplorpd *)pd_new(samplorpd_class);
    outlet_new(&x->x_obj, gensym("signal"));
    if (x)
    {
        //process the arguments :
        if ((argc >= 1) && (argv[0].a_type==A_FLOAT))
        {
            post("float: %f", argv[0].a_w.w_float);
            numoutputs = (long)argv[0].a_w.w_float;
        }
        else
            numoutputs = DEFAULT_NOUTPUTS;
        
        if ((argc >= 2) && (argv[1].a_type==A_FLOAT))
            maxvoices = (long)argv[1].a_w.w_float;
        else
            maxvoices = DEFAULT_MAXVOICES;

        //process numoutputs
        if (numoutputs < -2)  // stereo buffer and multipan
        {
            x->stereo_mode = 1;
            numoutputs = abs((int)numoutputs);
        }
        else if (numoutputs == -2)
        {
            x->stereo_mode = 1;
            numoutputs = 2;
        }
        else
            x->stereo_mode = 0;
        numoutputs = min(numoutputs,MAX_OUTPUTS);
        n = max(numoutputs,1);
        x->num_outputs = max(numoutputs,0);
#if 0
        /* INLETS the pd version has only one inlet */
        if (x->num_outputs == 3)
            floatin(x,7);
        if (x->num_outputs >= 2)
            floatin(x,6);
        floatin(x,5);
        floatin(x,4);
        intin(x,3);
        intin(x,2);
        intin(x,1);
#endif
        /* OUTLETS */
        x->right_outlet = outlet_new(&x->x_obj,NULL); //to report the number of active voices and the loop points
        while(n--)
            outlet_new(&x->x_obj,gensym("signal"));
        
        /* object initialisation */
        x->ctlp = &(x->ctl);
        samplor_init(x->ctlp);
        //  x->ctlp->list.samplors = (t_samplor_entry *)sysmem_newptr(maxvoices * sizeof(t_samplor_entry));
        x->ctlp->list.samplors = (t_samplor_entry *)malloc(maxvoices * sizeof(t_samplor_entry));
        samplor_maxvoices(x,maxvoices);
        x->time = 0;
        x->count = 0;
        x->dtd = 0;
        x->thread_safe_mode = 0;
        x->local_double_buffer = 1;
        x->buffer_mode = ARRAY;
        x->loop_release = 0;
        
        for (i=0;i<=x->num_outputs;i++)
            x->vectors[i] = NULL;
    }
    return(x);
}

/* 
 * class setup
 */
void samplorpd_tilde_setup(void)
{
    samplorpd_class = class_new(gensym("samplorpd~"), (t_newmethod)samplorpd_new, 0, sizeof(t_samplorpd), 0, A_GIMME, 0);

     // declares leftmost inlet as a signal inlet
    //CLASS_MAINSIGNALIN(samplorpd_class, t_samplorpd, x_f);

    class_addmethod(samplorpd_class, (t_method)samplor_maxvoices, gensym("maxvoices"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_int, gensym("int"),0);
    class_addmethod(samplorpd_class, (t_method)samplor_debug, gensym("debug"), A_FLOAT, 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("init"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_interpol, gensym("interpol"), A_FLOAT,0);
    class_addmethod(samplorpd_class, (t_method)samplor_set, gensym("set"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_list, gensym("list"), A_GIMME,0);
    class_addmethod(samplorpd_class, (t_method)samplor_bang, gensym("bang"), 0);
    class_addmethod(samplorpd_class,(t_method)samplor_play,gensym("play"),A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop_play,gensym("stop_play"),A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_adsr,gensym("adsr"), A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_adsr_ratio,gensym("adsr%"), A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop,gensym("stop"), A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop2,gensym("stop2"), A_DEFFLOAT, 0);
    class_addmethod(samplorpd_class,(t_method)samplor_stopall,gensym("stopall"), A_DEFFLOAT, 0);
    class_addmethod(samplorpd_class,(t_method)samplor_loop,gensym("loop"), A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_start,gensym("float"),A_FLOAT,0);
    class_addmethod(samplorpd_class,(t_method)samplor_count_active_voices,gensym("count"), A_DEFFLOAT, 0);

    //  class_addmethod(samplorpd_class,(t_method)samplor_buf,"in1",A_LONG,0);
    //  class_addmethod(samplorpd_class,(t_method)samplor_offset,"in2");
    //  class_addmethod(samplorpd_class,(t_method)samplor_dur, "in3");
    //  class_addmethod(samplorpd_class,(t_method)samplor_transp, "ft4");
    //  class_addmethod(samplorpd_class,(t_method)samplor_amp, "ft5");
    //  class_addmethod(samplorpd_class,(t_method)samplor_pan, "ft6");
    //  class_addmethod(samplorpd_class,(t_method)samplor_rev, "ft7");
    
    class_addmethod(samplorpd_class,(t_method)samplor_set_buffer_loop, gensym("buffer_loop"),    A_SYMBOL,A_FLOAT,A_FLOAT,0);
    class_addmethod(samplorpd_class,(t_method)samplor_get_buffer_loop,  gensym("get_buffer_loop"),    A_SYMBOL,0);
    class_addmethod(samplorpd_class,(t_method)samplor_modwheel, gensym("modwheel"),    A_FLOAT,0);
  
    // class_addmethod(samplorpd_class,(t_method)samplor_addsoundfile,gensym("addsf"),A_DEFSYMBOL,0);
    // class_addmethod(samplorpd_class,(t_method)samplor_listsoundfiles,gensym("listsf"),0);
    // class_addmethod(samplorpd_class,(t_method)samplor_getsoundfile,gensym("info"),A_DEFSYMBOL,0);
    // class_addmethod(samplorpd_class,(t_method)samplor_getmmap,gensym("mmapinfo"),A_DEFSYMBOLOL,0);
    // class_addmethod(samplorpd_class,(t_method)samplor_getmmap_sample,gensym("mmapval"),A_DEFSYMBOL,A_FLOAT,0);
    // class_addmethod(samplorpd_class,(t_method)samplor_clearsoundfile,gensym("clearsf"),A_DEFSYMBOL,0);
    // class_addmethod(samplorpd_class,(t_method)samplor_clearsoundfiles,gensym("clearallsf"),0);

    class_addmethod(samplorpd_class, (t_method)samplorpd_dsp, gensym("dsp"), 0);
    post("%s",VERSION);
    post("compiled %s %s",__DATE__, __TIME__);
}
