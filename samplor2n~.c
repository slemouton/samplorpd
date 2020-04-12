#include "m_pd.h"
#include "samplor2.h"
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define VERSION "samplor~: version for pd without flext v0.01 "

static t_class *samplor_class;

typedef struct _dspobj
{
    t_object x_obj;     /* obligatory header */
    t_float x_f;        /* place to hold inlet's value if it's set by message */
} t_dspobj;

/* METHODS */
/*
 * samplor_maxvoices sets the maximum number of voices
 */
void samplor_maxvoices(t_sigsamplor *x, long v)
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

/*-----------------------------------*/
static t_int *samplor_perform(t_int *w)
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

    /* called to start DSP.  Here we call Pd back to add our perform
    routine to a linear callback list which Pd in turn calls to grind
    out the samples. */
static void samplor_dsp(t_sigsamplor *x, t_signal **sp)
{
    dsp_add(samplor_perform, 3, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}


static void *samplor_new(void)
{
    //t_sigsamplor *x = (t_sigsamplor *)pd_new(samplor_class);
    //outlet_new(&x->x_obj, gensym("signal"));
    //return (x);

      t_dspobj *x = (t_dspobj *)pd_new(samplor_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

    /* this routine, which must have exactly this name (with the "~" replaced
    by "_tilde) is called when the code is first loaded, and tells Pd how
    to build the "class". */
void samplor2n_tilde_setup(void)
{
    samplor_class = class_new(gensym("samplor2n~"), (t_newmethod)samplor_new, 0,
    	sizeof(t_dspobj), 0, A_DEFFLOAT, 0);
	    /* this is magic to declare that the leftmost, "main" inlet
	    takes signals; other signal inlets are done differently... */
 //   CLASS_MAINSIGNALIN(samplor_class, t_sigsamplor, x_f);
 //   class_addmethod(samplor_class, (t_method)samplor_dsp, gensym("dsp"), 0);
     post("%s",VERSION);
   // post("compiled %s %s",__DATE__, __TIME__);
  //  class_addmethod(samplor_class, (t_method)samplor_maxvoices, gensym("maxvoices"), 0);
 /*   class_addint(c, samplor_int);
    class_addmethod(c, (t_method)samplor_set, gensym("set"), 0);
    class_addmethod(c, (t_method)samplor_debug, gensym("debug"), 0);
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
