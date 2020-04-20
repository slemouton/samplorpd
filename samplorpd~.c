#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"
#include "slm1.h"

#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif
#define VERSION "samplor~: version for pd without flext v0.0.82"

/* ------------------------ samplorpd~ ----------------------------- */

static t_class *samplorpd_class;


/* ------------------------ METHODS ----------------------------- */

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
    #if 1
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
 #endif
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
#if 0
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
#endif
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
#if 0
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
#endif
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
#if 0
    t_buffer_obj *buf = buffer_ref_getobject(x->ctlp->inputs.buf_ref);
    
    transp *= buffer_getsamplerate(buf) / x->ctlp->params.sr;
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
                    susloopstart = atom_getfloat(av+i+1);
                    susloopend = atom_getfloat(av+i+2);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"adsr"))
                {
                    x->ctlp->inputs.adsr_mode = 0;
                    samplor_win(x,0);
                    if (ac > 0) attack =  atom_getfloat(av+i+1);
                    if (ac > 1) decay = atom_getfloat(av+i+2);
                    if (ac > 2) sustain = atom_getfloat(av+i+3);
                    if (ac > 3) release = atom_getfloat(av+i+4);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"delay"))
                {
                    del = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"offset"))
                {
                    offset = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"dur"))
                {
                    dur = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"transp"))
                {
                    transp = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"pan"))
                {
                    pan = atom_getfloat(av+i+1);
                }
                else if (!strcmp((av + i)->a_w.w_sym->s_name,"aux"))
                {
                    rev = atom_getfloat(av+i+1);
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
#endif
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
            // samplor_bufname(x, (av + 1)->a_w.w_sym->s_name);
            samplor_bufname(x, atom_getsymbol(av + 1));
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
        post ("starting %s %f",atom_getsymbol(av + 1),atom_getfloat(av + 2));
        samplor_start(x, atom_getfloat(av));
}

/*
 * samplor_set set the sound buffer
 */
void samplor_set(t_samplorpd *x, t_symbol *s)
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

static void samplorpd_dsp(t_samplorpd *x, t_signal **sp)
{
    dsp_add(samplorpd_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
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
        #endif        
        /* OUTLETS */
        x->right_outlet = outlet_new(&x->x_obj,NULL); //to report the number of active voices and the loop points
        while(n--)
            outlet_new(&x->x_obj,gensym("signal"));
        
        /* object initialisation */
       #if 1
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

        for (i=0;i<=x->num_outputs;i++)
            x->vectors[i] = NULL;

     #endif
    }
    return(x);
}

/* 
 * class setup
 */
void samplorpd_tilde_setup(void)
{
    samplorpd_class = class_new(gensym("samplorpd~"), (t_newmethod)samplorpd_new, 0,
    	sizeof(t_samplorpd), 0, A_GIMME, 0);

    CLASS_MAINSIGNALIN(samplorpd_class, t_samplorpd, x_f);
 
    class_addmethod(samplorpd_class, (t_method)samplorpd_dsp, gensym("dsp"), 0);
    post("%s",VERSION);
    post("compiled %s %s",__DATE__, __TIME__);
    
    class_addmethod(samplorpd_class, (t_method)samplor_maxvoices, gensym("maxvoices"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_int, gensym("int"),0);
    class_addmethod(samplorpd_class, (t_method)samplor_debug, gensym("debug"), A_FLOAT, 0);
   // class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("init"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_interpol, gensym("interpol"), A_FLOAT,0);
 
    class_addmethod(samplorpd_class, (t_method)samplor_set, gensym("set"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_list, gensym("list"), A_GIMME,0);
    class_addmethod(samplorpd_class, (t_method)samplor_bang, gensym("bang"), 0);
    
#if 0
  //  class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("window"), 0);
    
    class_addmethod(samplorpd_class,(t_method)samplor_play,"play",A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop_play,"stop_play",A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_adsr, "adsr", A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_adsr_ratio, "adsr%", A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop, "stop", A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_stop2, "stop2", A_DEFLONG, 0);
    class_addmethod(samplorpd_class,(t_method)samplor_stopall, "stopall", A_DEFLONG, 0);
    class_addmethod(samplorpd_class,(t_method)samplor_loop, "loop", A_GIMME,0);
    class_addmethod(samplorpd_class,(t_method)samplor_start,"float",A_FLOAT,0);
    class_addmethod(samplorpd_class,(t_method)samplor_buf,"in1",A_LONG,0);
    class_addmethod(samplorpd_class,(t_method)samplor_offset,"in2");
    class_addmethod(samplorpd_class,(t_method)samplor_dur, "in3");
    class_addmethod(samplorpd_class,(t_method)samplor_transp, "ft4");
    class_addmethod(samplorpd_class,(t_method)samplor_amp, "ft5");
    class_addmethod(samplorpd_class,(t_method)samplor_pan, "ft6");
    class_addmethod(samplorpd_class,(t_method)samplor_rev, "ft7");
    class_addmethod(samplorpd_class,(t_method)samplor_set_buffer_loop,    "buffer_loop",    A_SYM,A_LONG,A_LONG,0);
    class_addmethod(samplorpd_class,(t_method)samplor_get_buffer_loop,    "get_buffer_loop",    A_SYM,0);
    class_addmethod(samplorpd_class,(t_method)samplor_modwheel,    "modwheel",    A_FLOAT,0);
    class_addmethod(samplorpd_class,(t_method)samplor_addsoundfile,    "addsf",A_DEFSYM,0);
    class_addmethod(samplorpd_class,(t_method)samplor_listsoundfiles, "listsf",0);
    class_addmethod(samplorpd_class,(t_method)samplor_getsoundfile, "info",A_DEFSYM,0);
    class_addmethod(samplorpd_class,(t_method)samplor_getmmap, "mmapinfo",A_DEFSYM,0);
    class_addmethod(samplorpd_class,(t_method)samplor_getmmap_sample, "mmapval",A_DEFSYM,A_LONG,0);
    class_addmethod(samplorpd_class,(t_method)samplor_clearsoundfile, "clearsf",A_DEFSYM,0);
    class_addmethod(samplorpd_class,(t_method)samplor_count_active_voices, "count", A_DEFLONG, 0);
    class_addmethod(samplorpd_class,(t_method)samplor_clearsoundfiles, "clearallsf",0);
 //   class_addmethod(samplorpd_class,(t_method)samplor_dsp64, "dsp64",    A_CANT, 0);
  //  class_addmethod(samplorpd_class,(t_method)samplor_assist, "assist",    A_CANT,0);
    
    #endif
}

