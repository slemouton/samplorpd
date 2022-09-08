/*************************/
/* Keygroup system to use with 
 the samplor~ object to make a sampler
 Serge lemouton ircam 2004
 
 Input : midinote(0-127) midivel(0-127)
 Output : buffer offset dur transp amp
 Set(Region) : buffername pitch pitchmin pitchmax [velmin velmax [starttime endtime [pan]]]
*/

#include "m_pd.h"
#include "string.h"
#include "math.h"
#include "libgen.h"
#include "max_types.h"
#include <libxml2/libxml/parser.h>
#include <libxml/tree.h>
#include "samplorkg.h"
#define VERSION "samplorkg noflext : version 3.12 - lemouton@ircam.fr"

static t_class *skg_class;

void samplorkg_setup(void)
{   
    skg_class = class_new(gensym("samplorkg"), (t_newmethod)skg_new, (t_method)skg_free, sizeof(Skg), 0L,0);
    
    class_addmethod(skg_class, (t_method)skg_bang, &s_bang,0);
    class_addmethod(skg_class, (t_method)skg_int, gensym("int"), 0);
    class_addmethod(skg_class, (t_method)skg_float,	&s_float,	A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_in1, gensym("in1"), A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_in2, gensym("in2"), A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_set, gensym("list"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_read, gensym("read"), A_DEFSYM, 0);
    class_addmethod(skg_class, (t_method)skg_read_hise, gensym("read_hise"), A_DEFSYM, 0);
    class_addmethod(skg_class, (t_method)skg_import, gensym("import"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_import_hise, gensym("import_hise"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_export, gensym("export"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_export, gensym("exportasxml"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_exportastext, gensym("exportastext"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_exportassfz, gensym("exportassfz"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_export_as_hise, gensym("export_as_hise"), A_GIMME, 0);
    class_addmethod(skg_class, (t_method)skg_print, gensym("print"),	A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_sustain, gensym("sustain"), A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_velcurve, gensym("velcurve"),	A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_midipgm, gensym("pgm"), A_FLOAT, 0);
    class_addmethod(skg_class, (t_method)skg_clear,	gensym("clear"), 0);
    class_addmethod(skg_class, (t_method)skg_debug,	gensym("debug"), A_FLOAT, 0);
     
    post("%s", VERSION);
    post("compiled %s %s",__DATE__, __TIME__);
}

void skg_int(Skg *m, long x)
{	
    if (x >= MINPITCH && x<= MAXPITCH)
    {
        m->pitchCurr = (double)x;
        if (m->midipgm)
            skg_output2(m);
        else
            skg_output(m);
    }
}

void skg_float(Skg *m, t_floatarg x)
{
    if (x >= MINPITCH && x<= MAXPITCH)
    {
        m->pitchCurr = x;
        if (m->midipgm)
            skg_output2(m);
        else
            skg_output(m);
    }
}

void skg_in1(Skg *m, t_float x)
{
    m->velCurr = (long) x;
}

void skg_in2(Skg *m, t_float x)
{
    m->channel = (long) x;
}

void skg_sustain(Skg *m,long x)
{
    m->sustain = (int)x;
}

void skg_velcurve(Skg *m,long x)
{
    m->velocity_compensation = (int) x;
}

void skg_midipgm(Skg *m,long x)
{
    m->midipgm = (int)x;
}

void skg_debug(Skg *m,long x)
{
    m->debug = (int)x;
}

void skg_print(Skg *m,t_float f)
{
    long chan = (long)f;
    int i,j;
    /*  for(i=0;i<m->tableSize -1 ;i++)
     {
     post("kg%d = ",i);
     post("%d %s %d %d %d %d %d",m->table[i].id,m->table[i].samplename,m->table[i].pitch,m->table[i].pitchMin,m->table[i].pitchMax,m->table[i].velMin,m->table[i].velMax);
     }
     */
    for(i=0;i<MAXPITCH - MINPITCH ;i++)
    {
        for (j=0;j<m->keymap[i][chan].nzones;j++)
            post("map%d = %s %f (%d) <%d %d> %f",i+MINPITCH,m->keymap[i][chan].zones[j].sample.name,m->keymap[i][chan].zones[j].sample.pitch,m->keymap[i][chan].nzones,m->keymap[i][chan].zones[j].velmin,m->keymap[i][chan].zones[j].velmax,m->keymap[i][chan].zones[j].sample.pan);
    }
}


void skg_clear(Skg *m)
{ int i,j;
    m->tableSize = 0;
    for(i = 0 ;i <= NCHANS;i++)
        for(j = 0 ;j < MAXPITCH - MINPITCH;j++)
        {
            m->keymap[j][i].zones[0].sample.pitch = 0.;
            m->keymap[j][i].nzones = 0;
            m->keymap[j][i].zones[0].velmin = 1;
            m->keymap[j][i].zones[0].velmax = 127;
        }
}

void skg_bang(Skg *m)
{
    if(m->mapflag)skg_make_map(m);
}

void skg_output(Skg *m)
{
    t_atom myList[7];
    float transp,interval;
    float velCurr ;
    int i,offset;
    int pitchCurrInt=  (int)m->pitchCurr ;
    int channel = m->channel;
    int nzones ;
    VelMapItem *z;
    SkgSample *sample[MAXVELZONES];
    SkgSample *s = 0;
    VelMapItem *velZone[MAXVELZONES];
    int nsample = 0;
    
    pitchCurrInt = MIN (pitchCurrInt,MAXPITCH);
    pitchCurrInt -= MINPITCH;
    nzones = m->keymap[pitchCurrInt][channel].nzones;
    z = m->keymap[pitchCurrInt][channel].zones;
    /* note off */
    //	if (m->sustain && !m->velCurr)
    if (m->velCurr == 0)
        for(i=0;i < nzones;i++)
        {
            velZone[nsample] = &z[i];
            sample[nsample++] = &(z[i].sample);
            if(m->debug)
                post ("noteoff");
        }
    else
    /* find vel zone */
        for(i=0;i < nzones;i++)
            if(m->velCurr >= z[i].velmin && m->velCurr <= z[i].velmax)
            {
                velZone[nsample] = &z[i];
                sample[nsample++] = &(z[i].sample);
                if(m->debug)
                    post ("zone %d %d ",z[i].velmin,z[i].velmax);
                
            }
    for(i=0;i<nsample;i++)
    {
        s=sample[i];
        //	interval =  m->pitchCurr - m->keymap[pitchCurrInt][channel].zones[0].sample.pitch ;
        interval =  m->pitchCurr - s->pitch ;
        transp =  s->tune * exp(.057762265 * interval );
        offset = 0;
        
        SETFLOAT(myList,0.);
        myList[1].a_type = A_SYMBOL;
        myList[1].a_w.w_symbol = gensym(s->name);
        SETFLOAT(myList+2,s->starttime);
        if (m->sustain)
        {
            SETFLOAT(myList+3,-1); /*-1 means looping */
        }
        else
            if (s->endtime == 0)
                SETFLOAT(myList+3,0);
            else
                SETFLOAT(myList+3,s->endtime - s->starttime);
        SETFLOAT(myList+4,transp);
        if (m->velocity_compensation == 1 && m->velCurr !=0 ) // velocity reference at the top of the velocity zone
        {
            velCurr = SCALE(m->velCurr,velZone[i]->velmin,velZone[i]->velmax,velZone[i]->velmin,127);
            velCurr = velCurr * velCurr / 16129.;
        }
        else if  (m->velocity_compensation == 2 && m->velCurr !=0 ) // velocity reference in the middle of the velocity zone
        {
            velCurr = SCALE(m->velCurr,velZone[i]->velmin, 0.5 *(velZone[i]->velmin + velZone[i]->velmax),velZone[i]->velmin,127);
            velCurr = velCurr * velCurr / 16129.;
        }
        else if  (m->velocity_compensation == 3 && m->velCurr !=0 ) // velocity reference in the middle of the velocity zone
        {
            velCurr = 256. * m->velCurr /(velZone[i]->velmin + velZone[i]->velmax);
            velCurr = velCurr * velCurr / 16129.;
        }
        else if  (m->velocity_compensation == 4 && m->velCurr !=0 ) // linear amplitude
        {
            velCurr = SCALE(m->velCurr,velZone[i]->velmin,velZone[i]->velmax,0.,1.);
        }
        else
        {
            velCurr = m->velCurr;
            velCurr = velCurr * velCurr / 16129.;
        }
        SETFLOAT(myList+5,s->sample_level * velCurr);
        
        if(s->pan >= 0)
        {
            SETFLOAT(myList + 6,s->pan);
        }
        else
            SETFLOAT(myList + 6,0.5);
        
        outlet_list(m->outlet,0L,7,&myList[0]);
    }
}

void skg_output2(Skg *m) // midipgm mode
{
    t_atom myList[7];
    float transp,interval;
    float velCurr ;
    int h,i,offset;
    int pitchCurrInt=  (int)m->pitchCurr ;
    int channel = m->midipgm;
    int nzones ;
    VelMapItem *z;
    SkgSample *sample[MAXVELZONES];
    SkgSample *s = 0;
    VelMapItem *velZone[MAXVELZONES];
    int nsample = 0;
    
    pitchCurrInt = MIN (pitchCurrInt,MAXPITCH);
    pitchCurrInt -= MINPITCH;
    
    /* note off */
    if (m->velCurr == 0)
        for (h =1;h<=NCHANS;h++)
        {
            nzones = m->keymap[pitchCurrInt][h].nzones;
            z = m->keymap[pitchCurrInt][h].zones;
            for(i=0;i < nzones;i++)
            {
                if (z[i].playing)
                {
                    velZone[nsample] = &z[i];
                    sample[nsample++] = &(z[i].sample);
                    if (h!= channel) {
                        z[i].playing = FALSE;
                    }
                    if(m->debug)
                        post ("noteoff2");
                }
            }
        }
    else
    /* find vel zone */
    {
        nzones = m->keymap[pitchCurrInt][channel].nzones;
        z = m->keymap[pitchCurrInt][channel].zones;
        for(i=0;i < nzones;i++)
            if(m->velCurr >= z[i].velmin && m->velCurr <= z[i].velmax)
            {
                velZone[nsample] = &z[i];
                sample[nsample++] = &(z[i].sample);
                z[i].playing = TRUE;
                if(m->debug)
                    post ("zone2 %d %d %d",z[i].velmin,z[i].velmax,sample);
            }
    }
    for(i=0;i<nsample;i++)
    {
        s=sample[i];
        interval =  m->pitchCurr - s->pitch ;
        transp =  s->tune * exp(.057762265 * interval );
        offset = 0;
        SETFLOAT(myList,0.);
        myList[1].a_type = A_SYMBOL;
        myList[1].a_w.w_symbol = gensym(s->name);
        SETFLOAT(myList+2,s->starttime);
        if (m->sustain)
        {
            SETFLOAT(myList+3,-1); /*-1 means looping */
        }
        else
            if (s->endtime == 0)
                SETFLOAT(myList+3,0);
            else
                SETFLOAT(myList+3,s->endtime - s->starttime);
        SETFLOAT(myList+4,transp);
        if (m->velocity_compensation == 1 && m->velCurr !=0 ) // velocity reference at the top of the velocity zone
            velCurr = SCALE(m->velCurr,velZone[i]->velmin,velZone[i]->velmax,velZone[i]->velmin,127);
        else if  (m->velocity_compensation == 2 && m->velCurr !=0 ) // velocity reference in the middle of the velocity zone
            velCurr = SCALE(m->velCurr,velZone[i]->velmin, 0.5 *(velZone[i]->velmin + velZone[i]->velmax),velZone[i]->velmin,127);
        else if  (m->velocity_compensation == 3 && m->velCurr !=0 ) // velocity reference in the middle of the velocity zone
            velCurr = 256. * m->velCurr /(velZone[i]->velmin + velZone[i]->velmax);
        else
            velCurr = m->velCurr;
        SETFLOAT(myList+5,s->sample_level * (velCurr * velCurr / 16129.));
        
        if(s->pan >= 0)
        {
            SETFLOAT(myList + 6,s->pan);
        }
        else
            SETFLOAT(myList + 6,0.5);
        
        outlet_list(m->outlet,0L,7,&myList[0]);
    }
}

float db2lin(float db)
{
    return (pow(10.,(db/20.)));
}

void skg_set(Skg *m, t_symbol *s, short ac, t_atom *av)
{
    int i = m->tableSize;
    if (i>= DIM)
    {
        error ("TOO MUCH GROUPS, max = %d", DIM);
        return;
    }

    if (ac >= 12)
        m->table[i].sample_level = db2lin((float)slm1_get_value(av + 11));
    else
        m->table[i].sample_level = 1.;
    
    if (ac >= 11)
        m->table[i].tune = (float)slm1_get_value(av + 10);
    else
        m->table[i].tune = 0.;
    
    if (ac >= 10)
        m->table[i].pan = (float)slm1_get_value(av + 9);
    else
        m->table[i].pan = -1.;
    
    if (ac > 4)
    {
        outlet_anything(m->textoutlet,av[1].a_w.w_symbol,0L,NIL);
        
        unsigned char pitchMin = MAX((unsigned char)slm1_get_value(av + 3),MINPITCH);
        unsigned char pitchMax = MIN((unsigned char)slm1_get_value(av + 4),MAXPITCH);
        pitchMin=MIN(pitchMin,MAXPITCH);
        if(m->debug)
            post("SET : id %d %s %f %d %d (%d)",(unsigned char)slm1_get_value(av),(av+1)->a_w.w_symbol->s_name, (double)slm1_get_value(av+2),pitchMin,pitchMax,ac);
        m->table[i].id = (unsigned char)slm1_get_value(av);
        if ( m->table[i].id >= NCHANS)
        {
            error ("TOO MUCH CHNNNELS, max = %d", NCHANS);
            return;
        }
        strcpy(m->table[i].samplename,(av+1)->a_w.w_symbol->s_name) ;
        m->table[i].pitch = (double)slm1_get_value(av + 2);
        m->table[i].pitchMin = pitchMin;
        if (pitchMax < pitchMin){unsigned char tmp = pitchMax; pitchMax = pitchMin;pitchMin = tmp;}
        m->table[i].pitchMax = pitchMax;
        
        if (ac >= 6)
        {
            unsigned char velMin = (unsigned char)slm1_get_value(av + 5);
            unsigned char velMax = (unsigned char)slm1_get_value(av + 6);
            if (velMax < velMin){unsigned char tmp = velMax; velMax = velMin;velMin = tmp;}
            if(m->debug)
                post("SET : vel min %d max %d",velMin,velMax);
            m->table[i].velMin = velMin;
            m->table[i].velMax = velMax;
            
            if (ac >= 9)
            {
                m->table[i].starttime = (int)slm1_get_value(av + 7);
                m->table[i].endtime = (int)slm1_get_value(av + 8);
                
            }
            else if (ac == 8)
                m->table[i].starttime = (int)slm1_get_value(av + 7);
            else
            {
                m->table[i].starttime = 0;
                m->table[i].endtime = 0;
            }
        }
        else
        {
            m->table[i].velMin = 1;
            m->table[i].velMax = 127;
        }
        m->tableSize++;
        m->mapflag = 1;
        if(m->debug)
            post("%d %s %f %d %d %d %d %f %f %f",m->table[i].id,m->table[i].samplename,m->table[i].pitch,m->table[i].pitchMin,m->table[i].pitchMax,m->table[i].velMin,m->table[i].velMax,m->table[i].pan,m->table[i].tune,m->table[i].sample_level);
    }
    else
    {
        error("key group syntax problem");
    }
}

void skg_make_map(Skg *m)
{
    int i,j,n;
    
    post("making map ... (%d keygroups)",m->tableSize);
    for(i = 0;i < m->tableSize ;i++)
    {
        int channel =  m->table[i].id;
        for(j = m->table[i].pitchMin - MINPITCH ;j <= (m->table[i].pitchMax - MINPITCH);j++)
        {
            n = (int) m->keymap[j][channel].nzones;
            if (n<MAXVELZONES)
            {
                m->keymap[j][channel].zones[n].sample.name = (char *)&m->table[i].samplename;
                m->keymap[j][channel].zones[n].sample.pitch = m->table[i].pitch;
                m->keymap[j][channel].zones[n].sample.starttime = m->table[i].starttime;
                m->keymap[j][channel].zones[n].sample.endtime = m->table[i].endtime;
                m->keymap[j][channel].zones[n].velmin=m->table[i].velMin;
                m->keymap[j][channel].zones[n].velmax = m->table[i].velMax;
                m->keymap[j][channel].zones[n].playing = FALSE;
                m->keymap[j][channel].zones[n].sample.pan = m->table[i].pan;
                m->keymap[j][channel].zones[n].sample.tune = powf(2., m->table[i].tune /12.);
                m->keymap[j][channel].zones[n].sample.sample_level = m->table[i].sample_level;
                m->keymap[j][channel].nzones++;
            }
            else
            {
                post("skg : trop de zones (n:%d i:%d j:%d c:%d)",n,i,j,channel);
            }
        }
    }
    m->mapflag = 0;
}

unsigned char gpitchMin,gpitchMax,gvelMin = 1,gvelMax = 127;
float gtune = 0.,gpan = 0.5;

static void parseM5p(Skg *m,xmlNode * a_node)
//parse a MachFive or Falcon Progrom
{
    xmlNode *cur_node = NULL;
    xmlAttr* attr;
    xmlChar* ac = NULL;
    char sampleName[256],sampleName2[256];
    int stereo_sample = 0;
    float pitch = 60.;
    float coarse = 0.;
    float fine = 0.;
    
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE)
        {
            if(m->debug)
                post("\t\t<<%s>>\n", cur_node->name);
            if (!strcmp((char *)cur_node->name, "UVI4"))
            {
                m->machfiveVersion = 3;
                post("Attention : fichier MachFive version 3 (UVI4)", cur_node->name);
            }
            if (!strcmp((char *)cur_node->name, "Keygroup"))
            {
                gvelMax = 127;
                attr = cur_node->properties;
                while(attr)
                {
                    ac = xmlGetProp(cur_node, attr->name);
                    
                    if(m->debug)
                        post("\t\t<%s><%s><%s>\n", cur_node->name,attr->name, ac);
                    
                    if (!strcmp((char *)attr->name, "HighKey"))
                        gpitchMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LowKey"))
                        gpitchMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "HighVelocity"))
                        gvelMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LowVelocity"))
                        gvelMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "Pan"))
                        gpan = 0.5 * (2. * strtof((char *)ac,NULL));
                    
                    if (!strcmp((char *)attr->name, "Tune"))
                    {
                        gtune = strtof((char *)ac,NULL);
                        if(m->debug)post("Tune %s --> %f is used in the preset",ac,gtune);
                    }
                    xmlFree(ac);
                    attr = attr->next;
                }
            }
            if (!strcmp((char *)cur_node->name, "SamplePlayer"))
            {
                attr = cur_node->properties;
                while(attr)
                {
                    ac = xmlGetProp(cur_node, attr->name);
                    if(m->debug)
                        post("[%d]<%s><%s>\n", m->machfiveVersion,attr->name, ac);
                    if (m->machfiveVersion == 1)
                    {
                        if (!strcmp((char *)attr->name, "SampleName"))
                        {
                            // si le nom commence par "*" -> fichiers stereo
                            if (strchr((char *)ac,'*'))
                            {
                                stereo_sample = 1;
                                strcpy(sampleName, strtok((char *) ac,"*"));
                                strcpy(sampleName2, strtok(NULL,"*"));
                            }
                            else
                                strcpy(sampleName,(char *)ac);
                        }
                    }
                    else if  (m->machfiveVersion == 3)
                    {
                        if (!strcmp((char *)attr->name, "SamplePath"))
                        {
                            // si le nom commence par "*" -> fichiers stereo
                            if (strchr((char *)ac,'*'))
                            {
                                stereo_sample = 1;
                                strcpy(sampleName, strtok((char *) ac,"*"));
                                strcpy(sampleName2, strtok(NULL,"*"));
                            }
                            else
                            {
                                strcpy(sampleName,(char *)ac);
                                char *pos ;
                                pos = strrchr(sampleName,'/');
                                if (pos)
                                {
                                    // post ("%s+%d-------------\n",pos+1,strlen(sampleName));
                                    strcpy(sampleName,pos+1);
                                }
                            }
                        }
                    }
                    if (!strcmp((char *)attr->name, "BaseNote"))
                        pitch = (double) strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "CoarseTune"))
                    { post ("coarse %f",(double) strtol((char *)ac,NULL,0));
                        coarse = (double) strtol((char *)ac,NULL,0);
                    }
                    if (!strcmp((char *)attr->name, "FineTune"))
                        fine = (double) strtol((char *)ac,NULL,0)/100.;
                    xmlFree(ac);
                    attr = attr->next;
                }
                if (gvelMax <= gvelMin)
                    gvelMax = 127;
                gtune = coarse + fine;
                skg_set_keygroup(m,m->channel,sampleName, pitch, gpitchMin, gpitchMax, gvelMin, gvelMax,gpan,gtune);
            }
        }
        parseM5p(m,cur_node->children);
    }
}

static void parseM5m(Skg *m,xmlNode * a_node)
//parse a Falcon Multi
{	
    xmlNode *cur_node = NULL;
    xmlAttr* attr;
    xmlChar* ac = NULL;
    char sampleName[256],sampleName2[256];
    int stereo_sample = 0;
    int channel = 1;
    float pitch = 60.;
    float coarse = 0.;
    float fine = 0.;
    
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE)
        {
            if(m->debug)
                post("\t\t<<%s>>\n", cur_node->name);
            if (!strcmp((char *)cur_node->name, "UVI4"))
            {
                m->machfiveVersion = 3;
                post("Attention : fichier MachFive version 3 (UVI4)", cur_node->name);
            }
            if (!strcmp((char *)cur_node->name, "Keygroup"))
            {
                post ("parsing program");
                channel ++;
                attr = cur_node->properties;
                while(attr)
                {
                    if(m->debug)
                        post("\t\t<%s><%s><%s>\n", cur_node->name,attr->name, ac);
                }
            }
                
            if (!strcmp((char *)cur_node->name, "Keygroup"))
            {
                gvelMax = 127;
                attr = cur_node->properties;
                while(attr)
                {
                    ac = xmlGetProp(cur_node, attr->name);
                    
                    if(m->debug)
                        post("\t\t<%s><%s><%s>\n", cur_node->name,attr->name, ac);
                    
                    if (!strcmp((char *)attr->name, "HighKey"))
                        gpitchMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LowKey"))
                        gpitchMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "HighVelocity"))
                        gvelMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LowVelocity"))
                        gvelMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "Pan"))
                        gpan = 0.5 * (2. * strtof((char *)ac,NULL));
                    
                    if (!strcmp((char *)attr->name, "Tune"))
                    {
                        gtune = strtof((char *)ac,NULL);
                        if(m->debug)post("Tune %s --> %f is used in the preset",ac,gtune);
                    }
                    xmlFree(ac);
                    attr = attr->next;
                }
            }
            if (!strcmp((char *)cur_node->name, "SamplePlayer"))
            {
                attr = cur_node->properties;
                while(attr)
                {
                    ac = xmlGetProp(cur_node, attr->name);
                    if(m->debug)
                        post("[%d]<%s><%s>\n", m->machfiveVersion,attr->name, ac);
                    if (m->machfiveVersion == 1)
                    {
                        if (!strcmp((char *)attr->name, "SampleName"))
                        {
                            // si le nom commence par "*" -> fichiers stereo
                            if (strchr((char *)ac,'*'))
                            {
                                stereo_sample = 1;
                                strcpy(sampleName, strtok((char *) ac,"*"));
                                strcpy(sampleName2, strtok(NULL,"*"));
                            }
                            else
                                strcpy(sampleName,(char *)ac);
                        }
                    }
                    else if  (m->machfiveVersion == 3)
                    {
                        if (!strcmp((char *)attr->name, "SamplePath"))
                        {
                            // si le nom commence par "*" -> fichiers stereo
                            if (strchr((char *)ac,'*'))
                            {
                                stereo_sample = 1;
                                strcpy(sampleName, strtok((char *) ac,"*"));
                                strcpy(sampleName2, strtok(NULL,"*"));
                            }
                            else
                            {
                                strcpy(sampleName,(char *)ac);
                                char *pos ;
                                pos = strrchr(sampleName,'/');
                                if (pos)
                                {
                                    // post ("%s+%d-------------\n",pos+1,strlen(sampleName));
                                    strcpy(sampleName,pos+1);
                                }
                            }
                        }
                    }
                    if (!strcmp((char *)attr->name, "BaseNote"))
                        pitch = (double) strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "CoarseTune"))
                    { post ("coarse %f",(double) strtol((char *)ac,NULL,0));
                        coarse = (double) strtol((char *)ac,NULL,0);
                    }
                    if (!strcmp((char *)attr->name, "FineTune"))
                        fine = (double) strtol((char *)ac,NULL,0)/100.;
                    xmlFree(ac);
                    attr = attr->next;
                }
                if (gvelMax <= gvelMin)
                    gvelMax = 127;
                gtune = coarse + fine;
                skg_set_keygroup(m,m->channel,sampleName, pitch, gpitchMin, gpitchMax, gvelMin, gvelMax,gpan,gtune);
            }
        }
        parseM5m(m,cur_node->children);
    }
}


void skg_set_keygroup(Skg *m,int id,char *sampleName,float pitch,int pitchMin,int pitchMax,int velMin,int velMax,float pan,float tune)
{
    t_atom myList[11];
    
    SETFLOAT(myList,id);
    myList[1].a_type = A_SYMBOL;
    myList[1].a_w.w_symbol = gensym(sampleName);
    SETFLOAT(myList+2,pitch);
    SETFLOAT(myList+3,pitchMin);
    SETFLOAT(myList+4,pitchMax);
    SETFLOAT(myList+5,velMin);
    SETFLOAT(myList+6,velMax);
    SETFLOAT(myList+7,0);
    SETFLOAT(myList+8,0);
    SETFLOAT(myList+9,pan);
    SETFLOAT(myList+10,tune);
    
    if(m->debug)
        post("SET tune %f",tune);
    skg_set(m,gensym("set"),11,&myList[0]);
}

void skg_read(Skg *x, t_symbol *s)
{
post ("not implemented yet in pure data");
#if 0
    defer(x, (t_method)skg_doread, s, 0, NULL);
#endif
}

void skg_doread(Skg *x, t_symbol *s)
{
post ("not implemented yet in pure data");
#if 0
    t_fourcc filetype = FOUR_CHAR_CODE('TEXT');
    t_fourcc outtype;
    char filename[512];
    char fullname[512];
    char fullname2[512];
    short path;
    if (s == gensym("")) { // if no argument supplied, ask for file
        if (open_dialog(filename, &path, &filetype, NULL, 0)) // non-zero: user cancelled
            return;
    } else {
        strcpy(filename, s->s_name); // must copy symbol before calling locatefile_extended
        if (locatefile_extended(filename, &path, &outtype, &filetype, 1)) { // non-zero: not found
            post( "%s: not found", s->s_name);
            return;
        }
    }
    // we have a file
    //	myobject_openfile(x, filename, path);
    path_topathname(path, filename, fullname);
    path_nameconform(fullname, fullname2, PATH_STYLE_MAX, PATH_TYPE_BOOT);
    if(x->debug)
        post("reading %s %d %s %s",filename,path,fullname2,fullname);
    skg_doimport(x, fullname2);
#endif
}

void skg_read_hise(Skg *x, t_symbol *s)
{
post ("not implemented yet in pure data");
#if 0
    defer(x, (t_method)skg_doread_hise, s, 0, NULL);
#endif

}

void skg_doread_hise(Skg *x, t_symbol *s)
{
post ("not implemented yet in pure data");
#if 0
    t_fourcc filetype = FOUR_CHAR_CODE('TEXT');
    t_fourcc outtype;
    char filename[512];
    char fullname[512];
    char fullname2[512];
    short path;
    if (s == gensym("")) { // if no argument supplied, ask for file
        if (open_dialog(filename, &path, &filetype, NULL, 0)) // non-zero: user cancelled
            return;
    } else {
        strcpy(filename, s->s_name); // must copy symbol before calling locatefile_extended
        if (locatefile_extended(filename, &path, &outtype, &filetype, 1)) { // non-zero: not found
            object_error((t_object *)x, "%s: not found", s->s_name);
            return;
        }
    }
    // we have a file
    //    myobject_openfile(x, filename, path);
    path_topathname(path, filename, fullname);
    path_nameconform(fullname, fullname2, PATH_STYLE_MAX, PATH_TYPE_BOOT);
    if(x->debug)
        post("reading %s %d %s %s",filename,path,fullname2,fullname);
    skg_doimport_hise(x, fullname2);
#endif
}

void skg_doimport(Skg *m, const char *file)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    
#ifdef LIBXML_TREE_ENABLED
    
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
    
    /*parse the file and get the DOM */
    doc = xmlReadFile(file, NULL, 0);
    
    if (doc == NULL) {
        printf("error: could not parse file %s\n", file);
    }
    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (doc == NULL) {	post("error: can't open file");}
    
 // post("parse a program");
    parseM5p(m,root_element);
    post("falcon multi mode");
 //   parseM5m(m,root_element);
    
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    
#else
    post("error : XML TREE NOT SUPPORTED");
#endif
    //	skg_bang(m);
    outlet_anything(m->textoutlet,gensym("done"),0L,NIL);
}

int skg_import(Skg *m,t_symbol *s, short ac, t_atom *av)
{
    //	skg_clear(m);
    m->machfiveVersion = 1;
    if(m->debug)
        post("importing %d %s",ac,av[0].a_w.w_symbol->s_name);
    if (ac != 1)
        return(1);
    skg_doimport(m,av[0].a_w.w_symbol->s_name);
    return 0;
}

static void parseHISE(Skg *m,xmlNode * a_node)
{
    post ("not implemented yet in pure data");
#if 0
    xmlNode *cur_node = NULL;
    xmlAttr* attr;
    xmlChar* ac = NULL;
    
    char sampleName[256];
    float pitch = 60.;
    float coarse = 0.;
    float fine = 0.;
    
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE)
        {
            if(m->debug)
                post("\t\t<<%s>>\n", cur_node->name);
            if (!strcmp((char *)cur_node->name, "samplemap"))
            {
            }
            if (!strcmp((char *)cur_node->name, "sample"))
            {
                attr = cur_node->properties;
                while(attr)
                {
                    ac = xmlGetProp(cur_node, attr->name);
                    if(m->debug)
                        post("\t\t<%s><%s><%s>\n", cur_node->name,attr->name, ac);
                    
                    if (!strcmp((char *)attr->name, "HiKey"))
                        gpitchMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LoKey"))
                        gpitchMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "HiVel"))
                        gvelMax = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "LoVel"))
                        gvelMin = strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "Pan"))
                        gpan = 0.5 * (2. * strtof((char *)ac,NULL));
                    if (!strcmp((char *)attr->name, "FileName"))
                    {
                        char value[256];
                        //sscanf((char *)ac, "{PROJECT_FOLDER}%[^\t\n]", value);
                        sscanf((char *)ac, "%[^\t\n]", value);
                        char* filename = basename(value);

                        if(m->debug)
                            post("value: %s\n", value);
                        strcpy(sampleName,filename);
                    }
                    if (!strcmp((char *)attr->name, "Pitch"))
                    {
                        gtune = strtof((char *)ac,NULL);
                        if(m->debug)object_post((t_object *)m,"Tune %s --> %f is used in the preset",ac,gtune);
                    }
                    if (!strcmp((char *)attr->name, "Root"))
                        pitch = (double) strtol((char *)ac,NULL,0);
                    if (!strcmp((char *)attr->name, "CoarseTune"))
                    {
                        if(m->debug)
                            post ("coarse %f",(double) strtol((char *)ac,NULL,0));
                        coarse = (double) strtol((char *)ac,NULL,0);
                    }
                    if (!strcmp((char *)attr->name, "FineTune"))
                        fine = (double) strtol((char *)ac,NULL,0)/100.;
                    
                    xmlFree(ac);
                    attr = attr->next;
                }
                
                //pitch = pitch - (coarse + fine);
                skg_set_keygroup(m,m->channel,sampleName, pitch, gpitchMin, gpitchMax, gvelMin, gvelMax,gpan,gtune);
                // if (stereo_sample)
                //    skg_set_keygroup(m,m->channel + 100,sampleName2, pitch, gpitchMin, gpitchMax, gvelMin, gvelMax);
            }
        }
        parseHISE(m,cur_node->children);
    }
#endif
    
}

void skg_doimport_hise(Skg *m, char *file)
{
    post ("not implemented yet in pure data");
#if 0
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    
//#ifdef LIBXML_TREE_ENABLED
    
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
    
    /*parse the file and get the DOM */
    doc = xmlReadFile(file, NULL, 0);
    
    if (doc == NULL) {
        printf("error: could not parse file %s\n", file);
    }
    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (doc == NULL) {    object_error((t_object *)m,"can't open file");}
    
    parseHISE(m,root_element);
    
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    
//#else
    object_error(m,"XML TREE NOT SUPPORTED");
//#endif
    //    skg_bang(m);
    outlet_anything(m->textoutlet,gensym("done"),0L,NIL);
#endif
}

int skg_import_hise(Skg *m,t_symbol *s, short ac, t_atom *av)
{
#if 0
    if(m->debug)
        post("importing %d %s",ac,av[0].a_w.w_symbol->s_name);
    if (ac != 1)
        return(1);
    skg_doimport_hise(m,av[0].a_w.w_symbol->s_name);
    #endif
    return 0;
}

void skg_export(Skg *m, t_symbol *s, short ac, t_atom *av)
{
    post ("not implemented yet in pure data");
#if 0
    int i, channel, export_channel = 1;
    char outs[512];
    if (ac==1)
        export_channel = (int) av[0].a_w.w_float;
    post ("export to Machfive format \nprogram %d",export_channel);
    // file header
    sprintf(outs,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    sprintf(outs,"<MachFiveProgram>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    if(m->debug)
        post("\nsyze %d",m->tableSize);
    //program header
    sprintf(outs,"<Program Name=\"Program\" Polyphony=\"16\" PortamentoTime=\"0.1\" ProgramName=\"exportedN%d.M5p\" ProgramPath=\"$/Users/lemouton/Projets/Co/exportedN%d.M5p\" Role=\"Gen\" Streaming=\"1\">",export_channel,export_channel);
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    sprintf(outs,"<Layer Color=\"red\" Name=\"Layer\" PolyphonicPortamentoTime=\"1\" Role=\"Gen\">");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    
    for(i = 0;i < m->tableSize ;i++)
    {
        channel =  m->table[i].id;
        if(m->debug)
            post ("%d %d %s",m->table[i].pitchMax,m->table[i].pitchMin,m->table[i].samplename);
        if(channel == export_channel)
        {
            sprintf(outs,"<Keygroup HighKey=\"%d\" LowKey=\"%d\" LowVelocity=\"1\" Name=\"%s\" Role=\"Gen\">",m->table[i].pitchMax,m->table[i].pitchMin,m->table[i].samplename);
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
            sprintf(outs,"<SamplePlayer BaseNote=\"%d\" Name=\"Oscillator\" Role=\"Gen\" SampleName=\"%s.aiff\" SamplePath=\".\" Streaming=\"1\"/>",(int)m->table[i].pitch,m->table[i].samplename);
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
            sprintf(outs,"</Keygroup>");
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
        }
    }
    //program tail
    sprintf(outs,"</Layer>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    sprintf(outs,"</Program>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    sprintf(outs,"</MachFiveProgram>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
#endif
}

void skg_exportastext(Skg *m, t_symbol *s, short ac, t_atom *av)
{
    int i, channel, export_channel = 0;
    char outs[512];
    if (ac==1)
        export_channel = (int) av[0].a_w.w_float;
    post ("export to text format  \nprogram %d",export_channel);
    // file header
    
    for(i = 0;i < m->tableSize ;i++)
    {
        channel =  m->table[i].id;
        if(m->debug)
            post ("%d %d %s",m->table[i].pitchMax,m->table[i].pitchMin,m->table[i].samplename);
        if((export_channel == 0) || (channel == export_channel))
        {
            sprintf(outs,"%d \"%s\" %f %d %d %d %d %d %d %f %f \n",channel, m->table[i].samplename, m->table[i].pitch, m->table[i].pitchMin, m->table[i].pitchMax, m->table[i].velMin,m->table[i].velMax,m->table[i].starttime,m->table[i].endtime,m->table[i].pan,m->table[i].tune);
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
        }
    }
}

void skg_exportassfz(Skg *m, t_symbol *s, short ac, t_atom *av)
{
    post ("not implemented yet in pure data");
#if 0
    int i, channel, export_channel = 0;
    char outs[512];
    if (ac==1)
        export_channel = (int) av[0].a_w.w_float;
    object_post ((t_object *)m,"export to text format  \nprogram %d",export_channel);
    // file header
    
    for(i = 0;i < m->tableSize ;i++)
    {
        channel =  m->table[i].id;
        if(m->debug)
            post ("%d %d %s",m->table[i].pitchMax,m->table[i].pitchMin,m->table[i].samplename);
        if((export_channel == 0) || (channel == export_channel))
        {
            sprintf(outs,"<region> lovel=%d hivel=%d key=%f lokey=%d hikey=%D tune=%f sample=%s\n",
                    m->table[i].velMin,m->table[i].velMax,m->table[i].pitch,m->table[i].pitchMin, m->table[i].pitchMax,m->table[i].tune,m->table[i].samplename);
            //  m->table[i].pitchMax, ,m->table[i].starttime,m->table[i].endtime,m->table[i].pan,m->table[i].tune);
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
        }
    }
#endif
}

void skg_export_as_hise(Skg *m, t_symbol *s, short ac, t_atom *av)
{
    post ("not implemented yet in pure data");
#if 0
    int i, channel, export_channel = 1;
    char outs[512];
    if (ac==1)
        export_channel = (int) av[0].a_w.w_float;
    object_post ((t_object *)m,"export to HISE keymap format \nprogram %d",export_channel);
    // file header
    sprintf(outs,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    sprintf(outs,"<samplemap ID=\"ExportedFromSamplor\" SaveMode=\"1\" RRGroupAmount=\"1\" MicPositions=\";\">\n");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
    
    for(i = 0;i < m->tableSize ;i++)
    {
        channel =  m->table[i].id;
        if(m->debug)
            post ("%d %d %s",m->table[i].pitchMax,m->table[i].pitchMin,m->table[i].samplename);
        if(channel == export_channel)
        {
            sprintf(outs,"<sample ID = \"%d\" FileName=\"{PROJECT_FOLDER}%s\" Root =\"%d\" \
                    \nHiKey=\"%d\" LoKey=\"%d\" LoVel=\"0\" HiVel=\"127\" RRGroup=\"1\" Volume=\"0\"  \
                    \nPan=\"0\" Normalized=\"0\" Pitch=\"0\" SampleStart=\"0\" SampleEnd=\"365677\"  \
                    \nSampleStartMod=\"0\" LoopStart=\"0\" LoopEnd=\"365677\" LoopXFade=\"0\" \
                    \nLoopEnabled=\"0\" LowerVelocityXFade=\"0\" UpperVelocityXFade=\"0\" \
                    \nSampleState=\"0\" NormalizedPeak=\"-1\" Duplicate=\"0\"/>", \
                    i,m->table[i].samplename,(int)m->table[i].pitch,m->table[i].pitchMax,m->table[i].pitchMin);
            outlet_anything(m->textoutlet,gensym(outs),0,NIL);
        }
    }
    //program tail
    sprintf(outs,"</samplemap>");
    outlet_anything(m->textoutlet,gensym(outs),0,NIL);
#endif
}

void skg_free(Skg *m)
{
    post ("not implemented yet in pure data");
#if 0
    int i,j;
    //	freebytes(m->table,sizeof(struct keygroup) * (long)DIM );
    sysmem_freeptr (m->table);
    for (i=0;i<=MAXPITCH-MINPITCH;i++)
    {
        for(j=0;j<=NCHANS;j++)
        {
            sysmem_freeptr(m->keymap[i][j].zones);
        }
    }
#endif
}

static void *skg_new(t_symbol *s, int argc, t_atom *argv)
{
    Skg *m = NULL;
    int i,j;
    
    m = (Skg *)pd_new(skg_class);
    inlet_new(&m->s__ob,&m->s__ob.ob_pd,&s_float,gensym("in1"));
    inlet_new(&m->s__ob,&m->s__ob.ob_pd,&s_float,gensym("in2"));
    m->outlet= outlet_new((t_object*) m,&s_list);
    m->textoutlet=outlet_new((t_object*)m,&s_symbol);
    m->samplor_outlet=outlet_new((t_object*)m,&s_list); // to preload soundfiles in samplor3
    
    if (!(m->table = (struct keygroup *)getbytes((sizeof(struct keygroup) * (long) DIM ))))
    {
        post("skg: not enough memory 1");
        return (0);
    }
    m->channel = 1;
#if DYNALLOC
    /*	skg_alloc(m);*/
    if (!(m->keymap=(struct keymapitem **)getbytes((long)( (MAXPITCH + 2 - MINPITCH) * (sizeof(struct keymapitem))))))
    {
        post("skg1: not enough memory 2");
        return (0);
    }
    for(i=0;i<(MAXPITCH + 2 - MINPITCH) ;i++)
        if (!(m->keymap[i]=(struct keymapitem *)getbytes((long)( (NCHANS + 1) * (sizeof(struct keymapitem))))))
        {
            post("skg1: not enough memory 2");
            return (0);
        }
#endif
    for (i=0;i<=MAXPITCH-MINPITCH;i++)
    {
        for(j=0;j<=NCHANS;j++)
        {
            if (!(m->keymap[i][j].zones = (struct velmapitem *)getbytes((sizeof(struct velmapitem) * (long) MAXVELZONES ))))
            {
                post("skg: not enough memory 2");
                return (0);
            }
        }
    }
    skg_clear(m);
    m->mapflag = 0;
    m->sustain = 0;
    m->velocity_compensation = 3;
    m->debug = 0;
    m->machfiveVersion=1;
    m->midipgm = 0;
    return (m);
}

#if DYNALLOC
void skg_alloc(Skg *m)
{
    
}
#endif

float slm1_get_value(t_atom *a)
{
    switch (a->a_type)
    {
        case A_FLOAT: return a->a_w.w_float;
        case A_SYMBOL:   if (*a->a_w.w_symbol->s_name == '#') break;
        default:      error("slm-1: illegal init argument");
    }
    return 0;
}
