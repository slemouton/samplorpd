//
//  samplorkg.h
//  samplor.kg
//
//  Created by serge lemouton on 26/11/2018.
//

#ifndef samplorkg_h
#define samplorkg_h
#endif /* samplorkg_h */

#define SCALE(x,a,b,c,d) ((d - c)*(x - a)/(b-a))+c
#ifndef MAX
#define MAX(x,y) ((x)>(y) ?(x):(y))
#endif
#ifndef MIN
#define MIN(x,y) ((x)<(y) ?(x):(y))
#endif

#define DIM 4000
#define MINPITCH 1
#define MAXPITCH 127
#define MAXVELZONES 128
#define NCHANS 64
#define DYNALLOC 1
#define FALSE 0
#define TRUE 1
#define NIL ((void *)0)

typedef struct keygroup
{
    unsigned char id;
    char samplename[256];
    double pitch;
    unsigned char pitchMin;
    unsigned char pitchMax;
    unsigned char velMin;
    unsigned char velMax;
    int starttime;
    int endtime;
    float pan;
    float tune;
    float sample_level;
} Keygroup;

typedef struct skgsample
{
    char *name;
    double pitch;
    int starttime;
    int endtime;
    float pan;
    float tune;
    float sample_level;
} SkgSample;

typedef struct velmapitem
{
    SkgSample sample;
    unsigned char velmin;
    unsigned char velmax;
    char playing;
} VelMapItem;

typedef struct keymapitem
{
    /* VelMapItem zones[MAXVELZONES];*/
    VelMapItem *zones;
    unsigned char nzones;
} KeyMapItem;

typedef struct skg
{
    t_object s__ob;
    Keygroup *table;
#if DYNALLOC
    KeyMapItem **keymap;
#else
    KeyMapItem keymap[2 + MAXPITCH - MINPITCH][NCHANS + 1];
#endif
    void *outlet;
    void *textoutlet;
    void *samplor_outlet;
    double pitchCurr;
    unsigned char velCurr;
    unsigned char channel;
    int tableSize;
    int mapflag;
    int sustain;   /* sustain flag */
    int velocity_compensation; // just to avoid sample normalisation
    int debug;
    int machfiveVersion;
    int midipgm;
} Skg;


/* prototypes */
float slm1_get_value(t_atom *a);
void skg_int(Skg *m, long x);
void skg_float(Skg *m, t_floatarg x);
void skg_set(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_print(Skg *m,t_float c);
void skg_clear(Skg *m);
void skg_find(Skg *m);
void skg_output(Skg *m);
void skg_output2(Skg *m);
void skg_bang(Skg *m);
void skg_tick(Skg *m);
void skg_stop(Skg *m);
void skg_in1(Skg *m, t_float n);
void skg_in2(Skg *m, t_float n);
void skg_sustain(Skg *m,long x);
void skg_velcurve(Skg *m,long x);
void skg_midipgm(Skg *m,long x);
void skg_debug(Skg *m,long x);
void skg_read(Skg *x, t_symbol *s);
void skg_doread(Skg *x, t_symbol *s);
void skg_read_hise(Skg *x, t_symbol *s);
void skg_doread_hise(Skg *x, t_symbol *s);
int skg_import(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_doimport(Skg *m, const char *file);
int skg_import_hise(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_doimport_hise(Skg *m, char *file);
void skg_set_keygroup(Skg *m,int id,char *sampleName,float pitch,int pitchMin,int pitchMax,int velMin,int velMax,float pan,float tune       );
void skg_export(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_exportastext(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_exportassfz(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_export_as_hise(Skg *m, t_symbol *s, short ac, t_atom *av);
void skg_free(Skg *m);
static void *skg_new(t_symbol *s, int argc, t_atom *argv);
void skg_make_map(Skg *m);
void skg_alloc(Skg *m);
void skg_assist(Skg *x, void *b, long m, long a, char *s);
