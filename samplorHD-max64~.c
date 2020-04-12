/**********************
 * Samplor the sampler
 * by Serge Lemouton
 * (c) 1999-2018 IRCAM
 **********************
 
 * STILL TO DO:
 * verifier que les points de bouclage ne sont pas en dehors du fichier
 * syntax : grammaire
 * possible optimisations :
 * pseudo-exponential releases
 * pas besoin de faire le test sur le mode d'interpolation à chaque buffer ... (use a callback)
 * optimise loop cross fades when in sample interpolation mode !!!
 * assign local variables (instead of accessing the object structure) in perform loops)
 
 * Versions :
 * 3.6 dtd allows 24 bit stereo samples
 * 3.5 dtd allows 24 bit samples
 * 3.4 correct loop crossfade for stereo buffers
 * 3.3 correct bug loop+modwheel
 * 3.2 public release - streaming version
 * 3.1 nouvelle version - seulement 64 bit
 * 3.11 pan multichannel stereo
 * 3.12 MAXOUTPUTS 32 -> 128
 * 3.13 nothing special
 * 3.14 internal buffers + remove max 5 perform methods
 * 3.141 static linking of the audiofile lib
 * 3.15 internal buffers
 * 3.16 attributes
 */

#define VERSION "samplor~ HD: version 3.61 (dev)"

#include "math.h"
#include <string.h>
#include <libgen.h>
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "buffer.h"
#include "audiofile.h"
#include "slm1.h"
#include "samplor.h"
#include "samplor_mmap.h"
#include "samplor_audiofile.h"
#include "include/libresample.h"
static t_symbol *ps_buffer;
static t_class *sigsamplor3_class = NULL;

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
    x->inputs.buf = 0;
    x->inputs.buf_ref = 0;
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
    x->buf_tab = (t_hashtab *)hashtab_new(0);//hashtable initialisation :
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

/*
 * samplor_run_one runs one voice and returns 0 if the voice can be freed and 1
 * if it is still needed, TOUT EST LA !
 */
int samplor_run_one64(t_samplor_entry *x, t_double **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_double *out1 = out[0];
    t_double *out2 = out[1];
    t_double *out3 = out[2];
    t_float sample = 0;
    t_float *tab,w_f_index,f;
    t_float amp_scale;
    long index,index2, w_index, frames, nc;
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
    else if ((tab = buffer_locksamples(x->buf_obj)))
    {
        frames = buffer_getframecount(x->buf_obj);
        nc = buffer_getchannelcount(x->buf_obj);
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
            index = (long)f;
            index2 = (long)x->fposition2;
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
                  t_buffer_info info;
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
int samplor_run_one64_int(t_samplor_entry *x, t_double **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_double *out1 = out[0];
    t_double *out2 = out[1];
    t_double *out3 = out[2];
    t_float sample = 0;
    int16_t *tab;
    t_float w_f_index,f;
    t_float amp_scale;
    long index,index2, w_index, frames, nc;
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
            index = (long)f;
            index2 = (long)x->fposition2;
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

int samplor_run_one64_mmap_int(t_samplor_entry *x, t_double **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel)
{
    t_double *out1 = out[0];
    t_double *out2 = out[1];
    t_double *out3 = out[2];
    t_float sample = 0;
    int16_t *tab;
    unsigned char *bytes;
    t_float w_f_index,f;
    t_float amp_scale;
    long index,index2, w_index, frames, nc;
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
                    t_buffer_info info;
                    //  buffer_getinfo(x->buf_obj,&info);
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
    
    tab = buffer_locksamples(x->buf_obj);
    if (!tab)
        goto zero;
    frames = buffer_getframecount(x->buf_obj);
    nc = buffer_getchannelcount(x->buf_obj);
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
                sampleL *= w_f_index;
                sampleR *= w_f_index;
#endif
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
#ifdef MULTIPAN
                out[x->chan2][samplecount] += sampleL * x->pan;
                out[x->chan][samplecount] += sampleL * (1. - x->pan);
                out[x->chan4][samplecount] += sampleR * x->rev;
                out[x->chan3][samplecount++] += sampleR * (1. - x->rev);
#else
                out[x->chan][samplecount++] += sampleL;
#endif
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
    
    if (x->mmap_buf)  /* direct to disk */
    {
        tab = x->mmap_buf->addr + x->mmap_buf->offset - x->mmap_buf->pa_offset;
        bytes = tab;
        frames = x->mmap_buf->b_frames;
        nc = x->mmap_buf->b_nchans;
        one_over_maxvalue = x->mmap_buf->one_over_b_maxvalue;
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
                sampleL *= w_f_index;
                sampleR *= w_f_index;
#endif
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
#ifdef MULTIPAN
                out[x->chan2][samplecount] += sampleL * x->pan;
                out[x->chan][samplecount] += sampleL * (1. - x->pan);
                out[x->chan4][samplecount] += sampleR * x->rev;
                out[x->chan3][samplecount++] += sampleR * (1. - x->rev);
#else
                out[x->chan][samplecount++] += sampleL;
#endif
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
        return 0;
}

int samplor_run_one_lite64(t_samplor_entry *x, t_double **out, int n,long interpol)
{
    t_double *out1 = out[0];
    t_float sample = 0.;
    float *tab,*tab_up,w_f_index,amp_scale;
    double f;
    long index, frames, nc;
    
    //internal buffer :
    if (x->samplor_buf)
    {
        tab = x->samplor_buf->f_samples ;
        tab_up = x->samplor_buf->f_upsamples ;
        frames = x->samplor_buf->b_frames;
        nc = x->samplor_buf->b_nchans;
    }
    else if ((tab = buffer_locksamples(x->buf_obj)))
    {
        frames = buffer_getframecount(x->buf_obj);
        nc = buffer_getchannelcount(x->buf_obj);
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
                        sample = tab[index];
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

int samplor_run_one_lite64_int(t_samplor_entry *x, t_double **out, int n,long interpol)
{
    t_double *out1 = out[0];
    t_float sample = 0.;
    float w_f_index;
    int16_t *tab;
    double f;
    long index, frames, nc;
    float one_over_maxvalue;
    
    if (x->samplor_buf)  /* est-ce necessaire (l'info est déja dans la structure "entry", non ? */
    {
        tab = x->samplor_buf->b_samples ;
        frames = x->samplor_buf->b_frames;
        nc = x->samplor_buf->b_nchans;
        one_over_maxvalue = x->samplor_buf->one_over_b_maxvalue;
    }
    else if ((tab = (int16_t*) buffer_locksamples(x->buf_obj)))
    {
        frames = buffer_getframecount(x->buf_obj);
        nc = buffer_getchannelcount(x->buf_obj);
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

int samplor_run_one_lite64_mmap_int(t_samplor_entry *x, t_double **out, int n,long interpol)
{
    t_double *out1 = out[0];
    t_float sample = 0.;
    float w_f_index;
    int16_t *tab;
    double f;
    long index, frames, nc;
    float one_over_maxvalue;
    
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
        return (0);
}
/*
 * samplor_run_all runs all voices and removes the finished voices
 * from the used list and puts them into the free list
 */
void samplor_run_all64(t_samplor *x, t_double **outs, long n,long num_outputs)
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

void samplor_run_all64_mmap_int(t_samplor *x, t_double **outs, int n,long num_outputs)
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

void samplor_run_all64_int(t_samplor *x, t_double **outs, int n,long num_outputs)
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


void samplor_run_all_stereo64_mmap(t_samplor *x, t_double **outs, long n,long num_outputs)
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

void samplor_run_all_stereo64(t_samplor *x, t_double **outs, long n,long num_outputs)
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

/* lite version : mono, no delay, no windows */
void samplor_run_all_lite64(t_samplor *x, t_double **outs, int n,long num_outputs)
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

void samplor_run_all_lite64_mmap_int(t_samplor *x, t_double **outs, int n,long num_outputs)
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

void samplor_run_all_lite64_int(t_samplor *x, t_double **outs, int n,long num_outputs)
{
    t_samplor_entry *prev = x->list.used;
    t_samplor_entry *curr = prev;
    
    while (curr != LIST_END)
    {
        if (samplor_run_one_lite64_int(curr, outs, n, x->interpol))
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

/*
 * samplor_error is called when we run out of voices, to avoid too much printing
 * we sample the output down
 */
void samplor_error(t_sigsamplor *current)
{
    if (current != (t_sigsamplor*)0)
    {
        current->count++;
        if (gettime() - current->time > 100)
        {
            error("samplor~: out of voice (%d @ %d)", current->count, gettime());
            current->count = 0;
            current->time = gettime();
        }
    }
}

/*
 * type d'interpolation
 */
t_max_err samplor_interpol_set(t_sigsamplor *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_atom_long interpol = atom_getlong(av);
        samplor_interpol(x,(int)interpol);
    }
    return MAX_ERR_NONE;
}

t_max_err samplor_interpol_get(t_sigsamplor *x, void *attr, long *ac, t_atom **av)
{
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return MAX_ERR_GENERIC;
        }
        atom_setlong(*av,  x->ctlp->interpol);
    }
    return MAX_ERR_NONE;
}

void samplor_interpol(t_sigsamplor *x, int interpol)
{
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

/*
 * temps de cross fade pour boucle (in samples)
 */
void samplor_loopxfade(t_sigsamplor *x, int loop_xfade)
{
    x->ctlp->loop_xfade = loop_xfade;
}

t_max_err samplor_loopxfade_set(t_sigsamplor *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_atom_long fadetime = atom_getlong(av);
        samplor_loopxfade(x,(int) fadetime);
    }
    return MAX_ERR_NONE;
}

t_max_err samplor_loopxfade_get(t_sigsamplor *x, void *attr, long ac, t_atom **av)
{   char alloc;
    
    atom_alloc(ac, av, &alloc);     // allocate return atom
    atom_setlong(*av, x->ctlp->loop_xfade);
    return 0;
}

/*
 * voice_stealing
 * 0 = no voice stealing, else a time in samples to do the fade out
 */
void samplor_voice_stealing(t_sigsamplor *x, int mode)
{
    x->ctlp->voice_stealing = mode;
}

/*
 * samplor_start triggers a voice at p seconds after next block begin
 */
void samplor_start(t_sigsamplor *x, float p)
{
    long start;
    t_samplor *ctlp = x->ctlp;
    t_buffer_obj *buf = buffer_ref_getobject(ctlp->inputs.buf_ref);
    
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
                    object_warn((t_object *) x,"too much voices : cancelling voice_stealing");
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
                    object_warn((t_object *) x,"too much voices : cancelling voice_stealing");
                    ctlp->voice_stealing = 0;
                }
            }
        }
    }
    else if(buffer_ref_exists(ctlp->inputs.buf_ref))
    {      /* n'alloue pas de voix si offset > duree du son */
        if (buffer_getmillisamplerate(buf) * ctlp->inputs.offset < buffer_getframecount(buf))
        {
            ctlp->inputs.transp *= buffer_getsamplerate(buf) / ctlp->params.sr;
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
                    object_warn((t_object *)x,"too much voices : cancelling voice_stealing");
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
samplor_bang(t_sigsamplor *x)
{
    samplor_start(x, 0.);
}

void
samplor_int(t_sigsamplor *x,long d)
{
}

/*
 * don't use
 */
void
samplor_manual_init(t_sigsamplor *x,long d)
{
    int i;
    object_post((t_object *)x,"init level %d",d);
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
        x->x_obj.z_misc = Z_NO_INPLACE;
    }
    for (i=0;i<=x->num_outputs;i++)
        x->vectors[i] = NULL;
}

void samplor_count_active_voices(t_sigsamplor *x,long d)
{
    int count;
    /*   samplist_display(&(x->ctlp->list)); */
    //post active voices to rightmost inlet
    if (d == 0)
        outlet_int(x->right_outlet,x->ctlp->active_voices);
    else if (d == 1){
        count = samplist_count(&(x->ctlp->list));
        outlet_int(x->right_outlet,count);}
    else if (d == 2){
        count = samplist_count(&(x->ctlp->list));
        object_post((t_object *)x,"active %d %d",count,x->ctlp->active_voices);
        outlet_int((t_object *)x->right_outlet,x->ctlp->active_voices);}
}

/*
 * samplor_maxvoices sets the maximum number of voices
 */
void
samplor_maxvoices(t_sigsamplor *x, long v)
{
    if (v > MAX_VOICES) {
        object_warn((t_object *)x,"samplor~: maxvoices out of range (%d), set to %d", v, MAX_VOICES);
        v = MAX_VOICES;
    }
    if (v < 1)
        v = 1;
    /* allocate space for the list */
    x->ctlp->list.samplors = (t_samplor_entry *)sysmem_resizeptr(x->ctlp->list.samplors,(2 + (2 * v)) * sizeof(t_samplor_entry));
    if (x->ctlp->list.samplors == NULL)
        error("problem allocating voices");
    x->ctlp->list.maxvoices = 2 + (2 * v);
    x->ctlp->polyphony = v;
    samplist_init(&(x->ctlp->list));
    x->ctlp->active_voices = 0;
}

t_max_err samplor_maxvoices_set(t_sigsamplor *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        t_atom_long maxvoices = atom_getlong(av);
        x->ctlp->list.samplors = (t_samplor_entry *)sysmem_newptr(maxvoices * sizeof(t_samplor_entry));
        samplor_maxvoices(x,maxvoices);
    }
    return MAX_ERR_NONE;
}

t_max_err samplor_maxvoices_get(t_sigsamplor *x, void *attr, long *ac, t_atom **av)
{
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return MAX_ERR_GENERIC;
        }
        atom_setlong(*av, x->ctlp->polyphony);
    }
    return MAX_ERR_NONE;
}


/*
 * samplor_set set the sound buffer
 */
void samplor_set(t_sigsamplor *x, t_symbol *s)
{
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
}

void samplor_deferredset(t_sigsamplor *x, t_symbol *s, long argc, t_atom *argv)
{
    samplor_set(x, atom_getsym(argv));
}

/*
 * samplor_buf set the sound buffer
 */
void samplor_buf(t_sigsamplor *x, int buf)
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
void samplor_bufname(t_sigsamplor *x, char *name)
{
    char *bufname = "sample100";
    bufname = name;
    x->ctlp->inputs.samplenumber = -1;
    samplor_set(x,gensym(bufname));
}

/*
 * samplor_offset
 */
void samplor_offset(t_sigsamplor *x, int offset)
{
    if (offset < 0)
        offset = 0;
    x->ctlp->inputs.offset = offset;
}

/*
 * samplor_dur set the duration
 */
void samplor_dur(t_sigsamplor *x, double dur)
{
    x->ctlp->inputs.dur = dur;
}

/*
 * samplor_transp
 */
void samplor_transp(t_sigsamplor *x, double transp)
{
    x->ctlp->inputs.transp = transp;
}

/*
 * samplor_amp
 */
void samplor_amp(t_sigsamplor *x, double amp)
{
    x->ctlp->inputs.amp = amp;
}

/*
 * samplor_pan
 */
void samplor_pan(t_sigsamplor *x, double pan)
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
void samplor_rev(t_sigsamplor *x, double rev)
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
void samplor_win(t_sigsamplor *x, int win)
{
    if ((win < 0) || (win > NUM_WINS))
        win = 0;
    x->ctlp->inputs.env = win;
    x->ctlp->inputs.attack = 2;
    x->ctlp->inputs.decay = 2;
    x->ctlp->inputs.sustain = 100;
    x->ctlp->inputs.release = 2;
}

t_max_err samplor_win_set(t_sigsamplor *x , void *attr, long *ac, t_atom *av)
{
    if (ac && av) {
        t_atom_long win = atom_getlong(av);
        samplor_win(x,(int)win);
    }
    return MAX_ERR_NONE;
}

t_max_err samplor_win_get(t_sigsamplor *x, void *attr, long *ac, t_atom **av)
{
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return MAX_ERR_GENERIC;
        }
        atom_setlong(*av, x->ctlp->inputs.env);
    }
    return MAX_ERR_NONE;
}

/*
 * samplor_winar : window + atack and decay
 */
void samplor_winar(t_sigsamplor *x, int win,int attack,int release)
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
samplor_attack(t_sigsamplor *x, int dur)
{  if (dur < 0)
    dur = 0;
    x->ctlp->inputs.attack = dur;
}
void
samplor_decay(t_sigsamplor *x, int dur)
{  if (dur < 0)
    dur = 0;
    x->ctlp->inputs.decay = dur;
}
void
samplor_sustain(t_sigsamplor *x, int val)
{  if (val < 0)
    val = 0;
    x->ctlp->inputs.sustain = val;
}
void
samplor_release(t_sigsamplor *x, int dur)
{
    if (dur < 0)
        dur = 0;
    x->ctlp->inputs.release = dur;
}

void
samplor_adsr(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
{
    x->ctlp->inputs.adsr_mode = 0;
    samplor_adsr_assign(x,ac,av);
}
void
samplor_adsr_ratio(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
{
    x->ctlp->inputs.adsr_mode = 1;
    samplor_adsr_assign(x,ac,av);
}
void
samplor_adsr_assign(t_sigsamplor *x,  short ac, t_atom *av)
{
    samplor_win(x,0);
    if (ac > 0) samplor_attack(x, slm1_get_value(av));
    if (ac > 1) samplor_decay(x, slm1_get_value(av + 1));
    if (ac > 2) samplor_sustain(x, slm1_get_value(av + 2));
    if (ac > 3) samplor_release(x, slm1_get_value(av + 3));
}

void samplor_curve(t_sigsamplor *x, double curve)
{
    double c = max(0.01,curve);
    x->ctlp->inputs.release_curve = (t_samplor_real)c;
}

t_max_err samplor_curve_set(t_sigsamplor *x , void *attr, long *ac, t_atom *av)
{
    if (ac && av) {
        t_atom_float curve = atom_getfloat(av);
        samplor_curve(x,curve);
    }
    return MAX_ERR_NONE;
}


t_max_err samplor_curve_get(t_sigsamplor *x, void *attr, long *ac, t_atom **av)
{
    if (ac && av) {
        char alloc;
        if (atom_alloc(ac, av, &alloc)) {
            return MAX_ERR_GENERIC;
        }
        atom_setfloat(*av, x->ctlp->inputs.release_curve);
    }
    return MAX_ERR_NONE;
}


void samplor_loop_points(t_sigsamplor *x, int start,int end)
{
    x->ctlp->inputs.susloopstart = start;
    x->ctlp->inputs.susloopend = end;
}

/*
 * utility to force the loop points in a buffer
 */
void samplor_get_buffer_loop(t_sigsamplor *x, t_symbol *buffer_s)
{
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
}

void samplor_set_buffer_loop(t_sigsamplor *x, t_symbol *buffer_s,long loopstart,long loopend)
{
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
}

void samplor_modwheel(t_sigsamplor *x, double transp)
{
    double transpo = max(0.1,transp);
    x->ctlp->modwheel = (t_samplor_real)transpo;
}

/*
 * to  stop  one sample (arret rapide)
 */
void samplor_stop2(t_sigsamplor *x, long time)
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
void samplor_stopall(t_sigsamplor *x, long time)
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
void samplor_stop(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
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
            sample = (int)slm1_get_value(av);
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
        {samplor_stop_one_voice(x,(int)slm1_get_value(av),slm1_get_value(av + 1));
        }
    }
}

void
samplor_stop_one_voice(t_sigsamplor *x, int sample,float transp)
{
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
}

void samplor_stop_one_voice_str(t_sigsamplor *x, t_symbol *buf_name,float transp)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    long time;
    t_buffer_obj *buf = buffer_ref_getobject(x->ctlp->inputs.buf_ref);
    
    transp *= buffer_getsamplerate(buf) / x->ctlp->params.sr;
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
                    float start2 = 1000. * (float)curr->loop_end / curr->buf->b_sr;
                    if(x->ctlp->debug)
                        post ("buf %s amp %f loop_end %d %f %f",curr->buf_name->s_name,curr->amplitude,curr->loop_end,curr->buf->b_sr/1000.,start2);
                    t_atom av[6];
                    atom_setlong(av, 0);
                    atom_setsym(av+1, curr->buf_name); //buffer
                    atom_setfloat(av+2,start2 ); // start
                    atom_setlong(av+3,0);               //dur
                    atom_setfloat(av+4,1 );       //transp
                    atom_setfloat(av+5,curr->amplitude);  //amp
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

void samplor_stop_play(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
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
    t_buffer_obj *buf = buffer_ref_getobject(x->ctlp->inputs.buf_ref);
    
    transp *= buffer_getsamplerate(buf) / x->ctlp->params.sr;
    if (ac > 1)
    {
        if (av->a_type == A_SYM )
            samplor_bufname(x, av->a_w.w_sym->s_name);
        else
            samplor_buf(x, slm1_get_value(av));
    }
    if (ac > 2)
        for (i = 1 ; i<ac;i++) // syntax parsing
        {
            if ((av + i)->a_type == A_SYM )
            {
                if (!strcmp((av + i)->a_w.w_sym->s_name,"loop"))
                {
                    susloopstart = slm1_get_value(av+i+1);
                    susloopend = slm1_get_value(av+i+2);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"adsr"))
                {
                    x->ctlp->inputs.adsr_mode = 0;
                    samplor_win(x,0);
                    if (ac > 0) attack =  slm1_get_value(av+i+1);
                    if (ac > 1) decay = slm1_get_value(av+i+2);
                    if (ac > 2) sustain = slm1_get_value(av+i+3);
                    if (ac > 3) release = slm1_get_value(av+i+4);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"delay"))
                {
                    del = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"offset"))
                {
                    offset = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"dur"))
                {
                    dur = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"transp"))
                {
                    transp = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"pan"))
                {
                    pan = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"aux"))
                {
                    rev = slm1_get_value(av+i+1);
                }
            }
        }
    while (curr != LIST_END)
    {
        if((!strcmp(av->a_w.w_sym->s_name,curr->buf_name->s_name))&&((float)curr->increment==transp)&&(susloopstart==(int)curr->loop_beg)&&(susloopend==(int)curr->loop_end)&&(curr->loop_flag!=FINISHING_LOOP))
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
void samplor_loop(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
{
    t_samplor_entry *prev = x->ctlp->list.used;
    t_samplor_entry *curr = prev;
    int sample;
    float susloopstart = 0., susloopend = 0.;
    if (ac > 0) susloopstart = (int)slm1_get_value(av);
    if (ac > 1) susloopend = slm1_get_value(av + 1);
    if (ac > 2)
    {
        sample = (int)slm1_get_value(av + 2);
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
void samplor_list(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
{
    float amp;
    
    if (ac > 1)
    {
        if ((av + 1)->a_type == A_SYM )
            samplor_bufname(x, (av + 1)->a_w.w_sym->s_name);
        else
            samplor_buf(x, slm1_get_value(av + 1));
    }
    if (ac > 2) samplor_offset(x, slm1_get_value(av + 2));
    if (ac > 3) samplor_dur(x, slm1_get_value(av + 3));
    if (ac > 4) samplor_transp(x, slm1_get_value(av + 4));
    if (ac > 5)  // linear amplitude
    {
        if ((amp = slm1_get_value(av + 5)) > 0.)
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
    if (ac > 6) samplor_pan(x, slm1_get_value(av + 6));
    if (ac > 7) samplor_rev(x, slm1_get_value(av + 7));
    samplor_loop_points(x, 0, 0);
    if (x->thread_safe_mode == 1)
    {
        t_atom ava[1];
        atom_setfloat(ava, slm1_get_value(av));
        defer_low((t_object *)x,(method)samplor_start,NULL,1,ava);
    }
    else
        samplor_start(x, slm1_get_value(av));
}

/*
 * samplor_play triggers one voice with syntax buffer_name [optional_keyword values*]*
 * keyword list = loop (2 args) adsr (4 args) delay (1) offset (1) dur (1) transp (1) amp (1) pan (1) rev (1)
 * attention si le nombre d'argument n'est pas bon !
 */
void samplor_play(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av)
{
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
            samplor_buf(x, slm1_get_value(av));
    }
    if (ac > 2)
        for (i = 1 ; i<ac;i++) // syntax parsing
        {
            if ((av + i)->a_type == A_SYM )
            {
                if (!strcmp((av + i)->a_w.w_sym->s_name,"loop"))
                {
                    samplor_loop_points(x,slm1_get_value(av+i+1),slm1_get_value(av+i+2));
                    samplor_dur(x,-1); // implicitly turn loop on
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"adsr"))
                {
                    x->ctlp->inputs.adsr_mode = 0;
                    samplor_win(x,0);
                    samplor_attack(x, slm1_get_value(av+i+1));
                    samplor_decay(x, slm1_get_value(av+i+2));
                    samplor_sustain(x, slm1_get_value(av+i+3));
                    samplor_release(x, slm1_get_value(av+i+4));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"delay"))
                {
                    del = slm1_get_value(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"offset"))
                {
                    samplor_offset(x,slm1_get_value(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"dur"))
                {
                    samplor_dur(x,slm1_get_value(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"transp"))
                {
                    samplor_transp(x,slm1_get_value(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"amp"))
                {
                    amp = slm1_get_value(av+i+1);
                    samplor_amp(x,amp);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"pan"))
                {
                    samplor_pan(x,slm1_get_value(av+i+1));
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"aux"))
                {
                    samplor_rev(x,slm1_get_value(av+i+1));
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
}

void samplor_addsoundfile(t_sigsamplor *x,t_symbol *s)
{
    if (x->dtd)
        samplor_add_mmap(x,s);
    else
        samplor_add_internal_buffer(x,s);
}

void samplor_clearsoundfiles(t_sigsamplor *x)
{
    t_hashtab *buf_tab = x->ctlp->buf_tab;
    if (x->dtd)
        samplor_free_mmap(x);
    else
        samplor_free_buffers(x);
    hashtab_clear(buf_tab);
}

void samplor_clearsoundfile(t_sigsamplor *x,t_symbol *s)
{
    if (x->dtd)
        samplor_free_onemmap(x,s);
    else
        samplor_freesoundfile(x,s);
}

void samplor_perform64N(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    long i,j;
    long n = sampleframes;
    t_samplor *x_ctl = x->ctlp;
    void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    for (i=0;i<(x->num_outputs);i++)
        x->vectors64[i] = outs[i];
    
    i = n;
    while(i--)
    {
        for (j=0;j<(x->num_outputs);j++)
            x->vectors64[j][i] = 0.;
    }
    (fun_ptr)(x_ctl, x->vectors64, n, x->num_outputs);
}

void samplor_perform64_3(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_samplor *x_ctl = x->ctlp;
    t_double *out1 = outs[0];
    t_double *out2 = outs[1];
    t_double *out3 = outs[1];
    t_double *samplor_outs[3];
    long n = sampleframes;
    long i = n;
    t_double *o1 = out1;
    t_double *o2 = out2;
    t_double *o3 = out3;
    samplor_outs[0] = out1;
    samplor_outs[1] = out2;
    samplor_outs[2] = out3;
    
    while(i--)
    {
        *o1++ = 0.;
        *o2++ = 0.;
        *o3++ = 0.;
    }
    samplor_run_all64(x_ctl, samplor_outs, n, 3);
}

void samplor_perform64_2(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_samplor *x_ctl = x->ctlp;
    t_double *out1 = outs[0];
    t_double *out2 = outs[1];
    t_double *samplor_outs[2];
    void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    long n = sampleframes;
    long i = n;
    t_double *o1 = out1;
    t_double *o2 = out2;
    samplor_outs[0] = out1;
    samplor_outs[1] = out2;
    
    while(i--)
    {
        *o1++ = 0.;
        *o2++ = 0.;
    }
    //    samplor_run_all64(x_ctl, samplor_outs, n, 2);
    (fun_ptr)(x_ctl, samplor_outs, n, 2);
}

void samplor_perform64_1(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_samplor *x_ctl = x->ctlp;
    t_double *out1 = outs[0];
    t_double *samplor_outs[1];
    void (*fun_ptr)(t_samplor *, t_double **, long ,long ) = userparam;
    long n = sampleframes;
    long i = n;
    t_double *o = out1;
    samplor_outs[0] = out1;
    while(i--)
        *o++ = 0.;
    (fun_ptr)(x_ctl, samplor_outs, n, 1);
}

void samplor_dsp64(t_sigsamplor *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    x->ctlp->params.sr = samplerate;
    x->ctlp->params.vs = maxvectorsize;
    if (x->num_outputs > 3)
    {
        if(x->stereo_mode == 1)
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64N, 0, samplor_run_all_stereo64);
        else
        {
            if (x->dtd)
                object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64N, 0, samplor_run_all64_mmap_int);
            else if (x->local_double_buffer == 1)
                object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64N, 0, samplor_run_all64);
            else
                object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64N, 0, samplor_run_all64_int);
            
        }
    }
    else if (x->num_outputs == 3)
        object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_3, 0, NULL);
    else if (x->num_outputs == 2)
    {
        if (x->stereo_mode == 1)
            if (x->dtd)
                object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_2, 0, samplor_run_all_stereo64_mmap);
            else
                object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_2, 0, samplor_run_all_stereo64);
            else
            {
                if (x->dtd)
                    object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_2, 0, samplor_run_all64_mmap_int);
                else if (x->local_double_buffer == 1)
                    object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_2, 0, samplor_run_all64);
                else
                    object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_2, 0, samplor_run_all64_int);
            }
    }
    else if (x->num_outputs == 1)
    {
        if (x->dtd)
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 0, samplor_run_all64_mmap_int);
        else if (x->local_double_buffer == 1)
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 0, samplor_run_all64);
        else
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 0, samplor_run_all64_int);
    }
    else
    {
        if (x->dtd)
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 1, samplor_run_all_lite64_mmap_int);
        else if (x->local_double_buffer == 1)
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 1, samplor_run_all_lite64);
        else
            object_method(dsp64, gensym("dsp_add64"), x, samplor_perform64_1, 2, samplor_run_all_lite64_int);
    }
}

/*
 * instance and class setup functions
 */

void *samplor_new(t_symbol *s,long argc, t_atom *argv)
{
    long n,i;
    long numoutputs;
    long maxvoices = DEFAULT_MAXVOICES;
    t_sigsamplor *x = NULL;
    
    x = (t_sigsamplor *) object_alloc(sigsamplor3_class);
    if (x)
    {
        dsp_setup((t_pxobject *)x, 0);
        
        //process the arguments :
        if ((argc >= 1) && (atom_gettype(&argv[0])==A_LONG))
            numoutputs = atom_getlong(&argv[0]);
        else
            numoutputs = DEFAULT_NOUTPUTS;
        if ((argc >= 2) && (atom_gettype(&argv[1])==A_LONG))
            maxvoices = atom_getlong(&argv[1]);
        else
            maxvoices = DEFAULT_NOUTPUTS;
        
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
        /* INLETS */
        if (x->num_outputs == 3)
            floatin(x,7);
        if (x->num_outputs >= 2)
            floatin(x,6);
        floatin(x,5);
        floatin(x,4);
        intin(x,3);
        intin(x,2);
        intin(x,1);
        /* OUTLETS */
        x->right_outlet = outlet_new(x,NULL); /*to report the number of active voices and the loop points*/
        while(n--)
            outlet_new(x,"signal");
        /* object initialisation */
        x->ctlp = &(x->ctl);
        samplor_init(x->ctlp);
        x->ctlp->list.samplors = (t_samplor_entry *)sysmem_newptr(maxvoices * sizeof(t_samplor_entry));
        samplor_maxvoices(x,maxvoices);
        x->time = 0;
        x->count = 0;
        x->dtd = 0;
        x->thread_safe_mode = 0;
        x->local_double_buffer = 1;
        x->x_obj.z_misc = Z_NO_INPLACE;
        for (i=0;i<=x->num_outputs;i++)
            x->vectors[i] = NULL;
        attr_args_process(x, argc, argv);
    }
    return (x);
}

void samplor_assist(t_samplor *x, void *b, long m, long a, char *s) // 4 final arguments are always the same for the assistance method
{
    if (m == ASSIST_INLET) { //inlet
        switch (a) {
            case 0:
                sprintf(s,"bang");
                break;
            case 1:
                sprintf(s,"sample");
                break;
            case 2:
                sprintf(s,"offset");
                break;
            case 3:
                sprintf(s,"dur");
                break;
            case 4:
                sprintf(s,"transp");
                break;
            case 5:
                sprintf(s,"amp");
                break;
            case 6:
                sprintf(s,"pan");
                break;
            case 7:
                sprintf(s,"aux");
                break;
            default:
                sprintf(s,"samplor");
                break;
        }
    }
    else {    // outlet
        sprintf(s, "samplor outlet n.%ld", a);
    }
}

void ext_main(void *r)
{
    t_class *c = class_new("samplor3~",(method)samplor_new,(method) dsp_free,(long)sizeof(t_sigsamplor), 0L,A_GIMME,0);
    class_addmethod(c,(method)samplor_int,"int",A_LONG,0);
    class_addmethod(c,(method)samplor_set, "set", A_SYM, 0);
    class_addmethod(c,(method)samplor_manual_init, "init", A_DEFLONG, 0);
    class_addmethod(c,(method)samplor_list, "list", A_GIMME,0);
    class_addmethod(c,(method)samplor_play,"play",A_GIMME,0);
    class_addmethod(c,(method)samplor_stop_play,"stop_play",A_GIMME,0);
    class_addmethod(c,(method)samplor_adsr, "adsr", A_GIMME,0);
    class_addmethod(c,(method)samplor_adsr_ratio, "adsr%", A_GIMME,0);
    class_addmethod(c,(method)samplor_stop, "stop", A_GIMME,0);
    class_addmethod(c,(method)samplor_stop2, "stop2", A_DEFLONG, 0);
    class_addmethod(c,(method)samplor_stopall, "stopall", A_DEFLONG, 0);
    class_addmethod(c,(method)samplor_loop, "loop", A_GIMME,0);
    class_addmethod(c,(method)samplor_bang,"bang",0);
    class_addmethod(c,(method)samplor_start,"float",A_FLOAT,0);
    class_addmethod(c,(method)samplor_buf,"in1",A_LONG,0);
    class_addmethod(c,(method)samplor_offset,"in2");
    class_addmethod(c,(method)samplor_dur, "in3");
    class_addmethod(c,(method)samplor_transp, "ft4");
    class_addmethod(c,(method)samplor_amp, "ft5");
    class_addmethod(c,(method)samplor_pan, "ft6");
    class_addmethod(c,(method)samplor_rev, "ft7");
    class_addmethod(c,(method)samplor_set_buffer_loop,    "buffer_loop",    A_SYM,A_LONG,A_LONG,0);
    class_addmethod(c,(method)samplor_get_buffer_loop,    "get_buffer_loop",    A_SYM,0);
    class_addmethod(c,(method)samplor_modwheel,    "modwheel",    A_FLOAT,0);
    class_addmethod(c,(method)samplor_addsoundfile,    "addsf",A_DEFSYM,0);
    class_addmethod(c,(method)samplor_listsoundfiles, "listsf",0);
    class_addmethod(c,(method)samplor_getsoundfile, "info",A_DEFSYM,0);
    class_addmethod(c,(method)samplor_getmmap, "mmapinfo",A_DEFSYM,0);
    class_addmethod(c,(method)samplor_getmmap_sample, "mmapval",A_DEFSYM,A_LONG,0);
    class_addmethod(c,(method)samplor_clearsoundfile, "clearsf",A_DEFSYM,0);
    class_addmethod(c,(method)samplor_clearsoundfiles, "clearallsf",0);
    class_addmethod(c,(method)samplor_dsp64, "dsp64",    A_CANT, 0);
    class_addmethod(c,(method)samplor_assist, "assist",    A_CANT,0);
    class_addmethod(c,(method)samplor_count_active_voices, "count", A_DEFLONG, 0);
    
    // attributes : arguments in samplor3 :
    
    CLASS_ATTR_LONG(c, "maxvoices", 0, t_samplor, polyphony);
    CLASS_ATTR_SAVE(c, "maxvoices", 0); // attribute saved with the patcher
    CLASS_ATTR_ACCESSORS(c, "maxvoices", (method)samplor_maxvoices_get, (method)samplor_maxvoices_set);// override default accessors
    CLASS_ATTR_FILTER_CLIP(c, "maxvoices", 0, MAX_VOICES);
    
    //flags
    CLASS_ATTR_CHAR(c, "streaming_mode", 0, t_sigsamplor, dtd);
    CLASS_ATTR_ENUMINDEX(c, "streaming_mode", 0, "OFF ON");
    CLASS_ATTR_STYLE_LABEL(c, "streaming_mode", 0, "onoff", "Direct To Disk");
    
    CLASS_ATTR_CHAR(c, "thread_safe_mode", 0, t_sigsamplor, thread_safe_mode);
    CLASS_ATTR_ENUMINDEX(c, "thread_safe_mode", 0, "OFF ON");
    CLASS_ATTR_STYLE_LABEL(c, "thread_safe_mode", 0, "onoff", "Thread safe buffers");
    
    CLASS_ATTR_CHAR(c, "local_double_buffer", 0, t_sigsamplor, local_double_buffer); // this is the (default) mode to pre-convert the sample to double : plus gourmand en memoire, mais moins en CPU
    CLASS_ATTR_ENUMINDEX(c, "local_double_buffer", 0, "OFF ON");
    CLASS_ATTR_STYLE_LABEL(c, "local_double_buffer", 0, "onoff", "double samples");
    CLASS_ATTR_SAVE(c,"local_double_buffer",0);
    
    CLASS_ATTR_CHAR(c, "loop_release", 0, t_sigsamplor, loop_release); // mode to play samples that add a release marker (for benoit M)
    CLASS_ATTR_ENUMINDEX(c, "loop_release", 0, "OFF ON");
    CLASS_ATTR_STYLE_LABEL(c, "loop_release", 0, "onoff", "use release markers in buffers");
    CLASS_ATTR_SAVE(c,"loop_release",0);
    
    CLASS_ATTR_FLOAT(c,"curve",0,t_samplor_inputs,release_curve);
    CLASS_ATTR_ACCESSORS(c, "curve", samplor_curve_get, samplor_curve_set);
    CLASS_ATTR_FILTER_CLIP(c, "curve", 0.01,4.);
    
    CLASS_ATTR_LONG(c,"window",0,t_samplor_inputs,env);
    CLASS_ATTR_ENUMINDEX(c, "window", 0, "no triangle rectangle crescendo decrescendo hamming");
    CLASS_ATTR_SAVE(c,"window",0);
    CLASS_ATTR_ACCESSORS(c, "window", samplor_win_get, samplor_win_set);
    CLASS_ATTR_LONG(c,"voice_stealing",0,t_samplor,voice_stealing);
    CLASS_ATTR_SAVE(c,"voice_stealing",0);
    CLASS_ATTR_ACCESSORS(c,"voice_stealing",0,samplor_voice_stealing);
    
    CLASS_ATTR_LONG(c,"interpol",0,t_samplor,interpol);
    CLASS_ATTR_ACCESSORS(c, "interpol", samplor_interpol_get, samplor_interpol_set);
    CLASS_ATTR_ENUMINDEX(c, "interpol", 0, "NO linear square cubic cubicforloops upsampling");
    CLASS_ATTR_SAVE(c,"interpol",0);
    CLASS_ATTR_FILTER_CLIP(c, "interpol", 0, 8)
    CLASS_ATTR_LONG(c,"loop_xfade",0,t_samplor,loop_xfade);
    CLASS_ATTR_ACCESSORS(c, "loop_xfade", samplor_loopxfade_get, samplor_loopxfade_set);
    CLASS_ATTR_SAVE(c,"loop_xfade",0);
    CLASS_ATTR_LONG(c,"debug",0,t_samplor,debug);
    CLASS_ATTR_SAVE(c,"debug",0);
    
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    sigsamplor3_class = c;
    ps_buffer = gensym("buffer~");
    post("%s (%s - %s)", VERSION,__DATE__,__TIME__);
}

void samplor_free(t_sigsamplor *x)
{
    dsp_free((t_pxobject *)x); /* always call dsp_free() at the beginning of the object free method !*/
    /*t_freebytes(x->ctlp->windows[0],sizeof(float) * sizeof(t_sample) * WIND_SIZE * NUM_WINS);*/
    sysmem_freeptr(x->ctlp->buf_tab);
    sysmem_freeptr(x->ctlp->list.samplors);
    samplor_free_buffers(x);
    object_free(x->ctlp->inputs.buf_ref);
}
