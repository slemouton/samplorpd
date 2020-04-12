#include "max_types.h"
#define DEBUG 0
#define MULTIPAN 1
#define DELAY_ACTIVE 0
#define WINAR 0                     /* Window + ADSR */
#define MAX_OUTPUTS 128
#define DEFAULT_SRATE 44100         /* default sampling rate for initialization */
#define DEFAULT_NOUTPUTS 2
#define DEFAULT_MAXVOICES 12
#define MAX_VOICES 1024             /* max # of voices */
#define WAITINGNOTES 24             /* max waiting notes (voice stealing) */
#define DEFAULT_VOICES 12           /* default number of voices */
#define WIND_SIZE 512               /* taille des fenetres */
#define NUM_WINS 7                  /* nombre de fenetres */
#define LIST_END ((void*)0)         /* end of list */
#define TEXT 0
#define BINARY 1
#define TEXT 0
#define IGNORE_LOOP 0            /* play all buffer ignoring loop */
#define LOOP 1                    /* loop mode for loop_flag */
#define ALLER_RETOUR_LOOP 2        /* forward-backward */
#define IN_ALLER_RETOUR_LOOP 3    /* in forward-backward loop */
#define FINISHING_LOOP 4    /* in forward-backward loop */
#define SAMPLOR_MAX_OUTCOUNT 16 /* maximum number of signal outlets */
#define THREAD_SAFE 1

typedef double t_samplor_real;    /* samplor calculation has to be in double */
typedef short t_int16;                  ///< a 2-byte int  @ingroup misc
typedef double t_double;

typedef struct _samplorbuffer
{
    t_symbol   *b_name;     ///< name of the buffer
    t_int16    *b_samples;  ///< stored with interleaved channels if multi-channel
    t_float    *f_samples;  ///< stored with interleaved channels if multi-channel
    t_float    *f_upsamples; /// oversampling for a better interpolation - use libresample library
    long       b_frames;    ///< number of sample frames (each one is sizeof(float) * b_nchans bytes)
    long       b_nchans;    ///< number of channels
    long       b_size;      ///< size of buffer in floats
    long       b_sr;        ///< sampling rate of the buffer
    long       b_framesize;
    long       b_maxvalue;
    t_float    one_over_b_maxvalue; /// for optimisation !!
} t_samplorbuffer;


typedef struct _samplormmap
{
    t_symbol   *b_name;         /// name of the buffer
    int        fd;              /// file descriptor
    off_t offset;
    off_t pa_offset;            /// page offset
    size_t length;
    long index_start;           /// index du premier echantillon en memoire
    long index_end;             /// index du dernier echantillon en memoire
    char *addr;
    off_t      b_st_size;       /// file size as reported by fstat
    off_t      b_data_offset;   /// offset before the data as reported by libaudiofile
    int        byteOrder;
    long       b_frames;        /// number of sample frames (each one is sizeof(float) * b_nchans bytes)
    long       b_nchans;        /// number of channels
    long       b_size;          /// size of buffer in floats
    long       b_sr;            /// sampling rate of the buffer
    long       b_framesize;
    long       b_maxvalue;
    t_float    one_over_b_maxvalue; /// for optimisation !!
} t_samplormmap;


typedef struct _samplor_inputs {    /* samplor inputs */
    t_symbol *buf_name;
    t_buffer *buf; /* kept only bicoz of loop points */
    t_buffer_ref *buf_ref;
    t_samplorbuffer *samplor_buf;
    t_samplormmap *samplor_mmap;
    int samplenumber;
    t_int offset;            /* position in ms */
    t_float dur;            /* duration in ms */
    t_int envattack;        /* duration in ms */
    t_int envrelease;        /* duration in ms */
    t_int adsr_mode;        /* 0 (default) adsr in ms, 1 -> adsr in % of sound duration */
    t_int attack;            /* duration in ms */
    t_int decay;            /* duration in ms */
    t_int sustain;            /* value in % of amplitude */
    t_int release;            /* duration in ms */
    t_int susloopstart;      /*loop points in samples (new in version 2.91)*/
    t_int susloopend;
    t_samplor_real transp;
    t_float amp;
    t_float pan;
    t_float rev;
    t_int env;
    t_int chan;
    t_int chan2;
    t_int chan3;
    t_int chan4;
    t_samplor_real release_curve;
} t_samplor_inputs;

typedef struct _samplor_params {    /* samplor parameters and pre-calculated values */
    t_samplor_real sr;            /* sampling rate in Hz */
    t_samplor_real sp;            /* sampling period in s */
    long vs;                    /* vector size */
} t_samplor_params;

#include "linkedlist.h"

typedef struct _samplor {                /* samplor control structure */
    t_samplor_inputs inputs;            /* samplor inputs */
    t_samplor_params params;            /* samplor parameters */
    t_float windows[NUM_WINS][WIND_SIZE];/* grain envelope */
    t_samplor_list list;                /* samplor list */
    t_list waiting_notes;                /* notes en attente */
    long interpol;                        /* special mode */
    long voice_stealing;                /* special mode */
    long loop_xfade;                    /* special mode */
    long debug;
    long active_voices;  /* nombre de voix actives */
    long polyphony;  /* nombre de voix actives */
    t_samplor_real modwheel;
    long n_sf;
//    t_hashtab *buf_tab;
} t_samplor;

typedef struct _sigsamplor {
    t_object x_obj;
    t_samplor *ctlp;
    t_samplor ctl;
    void *right_outlet;
    long num_outputs;            /* nombre de sorties (1,2 ou 3) */
    long stereo_mode;
    t_float *vectors[SAMPLOR_MAX_OUTCOUNT+1]; 
    t_double *vectors64[SAMPLOR_MAX_OUTCOUNT+1]; 
    long time;
    long count;
    char thread_safe_mode; // attribut
    char local_double_buffer;  // attribut : plus gourmand en memoire mais moins en CPU
    char dtd;  // attribut : experimental streaming mode
} t_sigsamplor;

// OVERSAMPLING :
#define UPSAMPLING 0
enum {CLOSEST,LINEAR,SQUARE,CUBIC,CUBIC2,UPSAMPLING2,UPSAMPLING4,UPSAMPLING8};
/******************************************************************/
/* Prototypes */
t_samplor_entry *samplor_free_voice(t_samplor *x, t_samplor_entry *prev, t_samplor_entry *curr);
void samplor_init(t_samplor *x);
void samplor_windows(t_samplor *x);
int samplor_voice_alloc(t_samplor *x);
int samplor_voice_append(t_samplor *x);
void samplor_trigger(t_samplor *x, long start,t_samplor_inputs inputs);
int samplor_run_one64(t_samplor_entry *x, t_double **out, long n, const t_float *windows, long num_outputs,long interpol,long loop_xfade,t_samplor_real modwheel);
int samplor_run_one(t_samplor_entry *x, t_float **out, long n, t_float *windows, long num_outputs,long interpol,long loop_xfade);
int samplor_run_one_lite(t_samplor_entry *x, t_float **out, int n, long interpol);
void samplor_run_all(t_samplor *x, t_float **outs, long n,long num_outputs);
void samplor_error(t_sigsamplor *current); 
void samplor_voice_stealing(t_sigsamplor *x, int mode);
void samplor_list(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_play(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_voice_stealing(t_sigsamplor *x, int mode);
void samplor_start(t_sigsamplor *x, float p);
void samplor_bang(t_sigsamplor *x);
void samplor_int(t_sigsamplor *x,long d);
void samplor_debug(t_sigsamplor *x,long d);
void samplor_maxvoices(t_sigsamplor *x, long v);
void samplor_interpol(t_sigsamplor *x, int interpol);
void samplor_loopxfade(t_sigsamplor *x, int loop_xfade);
void samplor_buf(t_sigsamplor *x, int buf);
void samplor_bufname(t_sigsamplor *x, char *name);
void samplor_offset(t_sigsamplor *x, int offset);
void samplor_dur(t_sigsamplor *x, double dur);
void samplor_transp(t_sigsamplor *x, double transp);
void samplor_amp(t_sigsamplor *x, double amp);
void samplor_pan(t_sigsamplor *x, double pan);
void samplor_rev(t_sigsamplor *x, double rev);
void samplor_win(t_sigsamplor *x, int win);
void samplor_winar(t_sigsamplor *x, int win,int attack,int release);
void samplor_attack(t_sigsamplor *x, int time);
void samplor_decay(t_sigsamplor *x, int dur);
void samplor_sustain(t_sigsamplor *x, int val);
void samplor_release(t_sigsamplor *x, int dur);
void samplor_adsr(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_adsr_ratio(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_loop_points(t_sigsamplor *x, int start, int end);
void samplor_adsr_assign(t_sigsamplor *x,  short ac, t_atom *av);
void samplor_urgent_stop(t_samplor *x, long time);
void samplor_stop_oldest(t_samplor *x, long time);
void samplor_stop(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_stop2(t_sigsamplor *x, long time);
void samplor_stop_one_voice(t_sigsamplor *x, int sample,float transp);
void samplor_stop_play(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_stopall(t_sigsamplor *x, long time);
void samplor_loop(t_sigsamplor *x, t_symbol *s, short ac, t_atom *av);
void samplor_set(t_sigsamplor *x, t_symbol *s);
float samplor_get_value(struct atom *a);
void samplor_modwheel(t_sigsamplor *x, double transp);
void samplor_curve(t_sigsamplor *x, double curve);
t_int *samplor_perform3(t_int *w);
t_int *samplor_perform2(t_int *w);
t_int *samplor_perform1(t_int *w);
t_int *samplor_perform0(t_int *w);
void samplor_perform64_0(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64_1(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64_2(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64_3(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64N(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64_stereo(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_perform64StereoN(t_sigsamplor *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void samplor_get_buffer_loop(t_sigsamplor *x, t_symbol *buffer_name);
void samplor_set_buffer_loop(t_sigsamplor *x, t_symbol *buffer_name,long loopstart,long loopend);
void samplor_dsp(t_sigsamplor *x, t_signal **sp, short *count);
void samplor_free(t_sigsamplor *x);
void samplor_free_buffers(t_sigsamplor *x);
void *samplor_new0(long numoutputs,long maxgrains);
void *samplor_new(t_symbol *s,long argc, t_atom *argv);
t_max_err samplor_numoutputs_set(t_sigsamplor *x, void *attr, long ac, t_atom *av);
t_max_err samplor_maxvoices_set(t_sigsamplor *x, void *attr, long ac, t_atom *av);
void samplor_assist(t_samplor *x, void *b, long m, long a, char *s);
int main(void);

