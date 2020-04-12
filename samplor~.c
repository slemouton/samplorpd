#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define VERSION "samplor~: version for pd without flext v0.000"

/* METHODS */
/*
 * samplor_maxvoices sets the maximum number of voices
 */
void samplor_maxvoices(t_sigsamplor *x, long v)
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
    /*
    x->ctlp->active_voices = 0;
    */
}


/*-----------------------------------*/

static t_class *samplor_class;

    /* this is the actual performance routine which acts on the samples.
    It's called with a single pointer "w" which is our location in the
    DSP call list.  We return a new "w" which will point to the next item
    after us.  Meanwhile, w[0] is just a pointer to dsp-perform itself
    (no use to us), w[1] and w[2] are the input and output vector locations,
    and w[3] is the number of points to calculate. */

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
    t_sigsamplor *x = (t_sigsamplor *)pd_new(samplor_class);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_f = 0;
    return (x);
}

    /* this routine, which must have exactly this name (with the "~" replaced
    by "_tilde) is called when the code is first loaded, and tells Pd how
    to build the "class". */
void samplor_tilde_setup(void)
{
    samplor_class = class_new(gensym("samplor~"), (t_newmethod)samplor_new, 0,
    	sizeof(t_sigsamplor), 0, A_DEFFLOAT, 0);
	    /* this is magic to declare that the leftmost, "main" inlet
	    takes signals; other signal inlets are done differently... */
    CLASS_MAINSIGNALIN(samplor_class, t_sigsamplor, x_f);
    	/* here we tell Pd about the "dsp" method, which is called back
	when DSP is turned on. */
    class_addmethod(samplor_class, (t_method)samplor_dsp, gensym("dsp"), 0);
    post("%s",VERSION);
 //   post("compiled %s %s",__DATE__, __TIME__);
  //  class_addmethod(samplor_class, (t_method)samplor_maxvoices, gensym("maxvoices"), 0);

}
