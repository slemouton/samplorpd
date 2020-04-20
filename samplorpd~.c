#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"
#include "slm1.h"

#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif
#define VERSION "samplor~: version for pd without flext v0.0.81 "

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
#if 1
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
     #if 0 
    class_addmethod(samplorpd_class, (t_method)samplor_set, gensym("set"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("xfade"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("voice_stealing"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("list"), 0);    
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("window"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("adsr"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("stop"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("stop2"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("stopall"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("loop"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("buffer_loop"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_manual_init, gensym("get_buffer_loop"), 0);
    class_addmethod(samplorpd_class, (t_method)samplor_bang, gensym("bang"), 0);
    #endif
}

