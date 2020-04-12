#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"


#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif
#define VERSION "samplor~: version for pd without flext v0.015 "

/* ------------------------ samplorpd~ ----------------------------- */

static t_class *samplorpd_class;

typedef struct _samplorpd
{
    t_object x_obj; 	/* obligatory header */
    t_float x_f;    	/* place to hold inlet's value if it's set by message */
    t_samplor *ctlp;
    t_samplor ctl;
    long num_outputs;           /* nombre de sorties (1,2 ou 3) */
    long stereo_mode;
    t_sample *vectors[SAMPLOR_MAX_OUTCOUNT+1]; 
    long time;
    long count;
} t_samplorpd;

/* ------------------------ METHODS ----------------------------- */

void samplor_int(t_samplorpd *x,long d)
{
}

void samplor_debug(t_samplorpd *x,long d)
{
    x->ctlp->debug = d;
    post("debug %d",d);
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

static void *samplorpd_new0(void)
{
    t_samplorpd *x = (t_samplorpd *)pd_new(samplorpd_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

static void *samplorpd_new(t_symbol *s, long ac, t_atom *av)
{
   long n,i;
    long numoutputs;
    long maxvoices = DEFAULT_MAXVOICES;
    t_samplorpd *x = NULL;

    x = (t_samplorpd *)pd_new(samplorpd_class);
       outlet_new(&x->x_obj, gensym("signal"));
    if (x)
    {

    }
    return(x);

}


/* 
 * class setup
 */
void samplorpd_tilde_setup(void)
{
    samplorpd_class = class_new(gensym("samplorpd~"), (t_newmethod)samplorpd_new, 0,
    	sizeof(t_samplorpd), 0, A_DEFFLOAT, 0);

    CLASS_MAINSIGNALIN(samplorpd_class, t_samplorpd, x_f);
 
    class_addmethod(samplorpd_class, (t_method)samplorpd_dsp, gensym("dsp"), 0);
    post("%s",VERSION);
    post("compiled %s %s",__DATE__, __TIME__);
    
    class_addmethod(samplorpd_class, (t_method)samplor_maxvoices, gensym("maxvoices"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_int, gensym("int"),0);
    class_addmethod(samplorpd_class, (t_method)samplor_debug, gensym("debug"), 0);

  /* class_addmethod(c, (t_method)samplor_set, gensym("set"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("init"), 0);
    class_addmethod(c, (t_method)samplor_interpol, gensym("interpol"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("xfade"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("voice_stealing"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("list"), 0);    
    class_addmethod(c, (t_method)samplor_manual_init, gensym("window"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("adsr"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("stop"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("stop2"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("stopall"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("loop"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("buffer_loop"), 0);
    class_addmethod(c, (t_method)samplor_manual_init, gensym("get_buffer_loop"), 0);
    class_addmethod(c, (t_method)samplor_bang, gensym("bang"), 0);
    */
}
