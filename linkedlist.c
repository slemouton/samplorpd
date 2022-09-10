/*********************
	LINKED LIST
**********************/

//#include "ext.h"
//#include "z_dsp.h"
//#include "buffer.h"
#include <stdlib.h>
#include "m_pd.h"
#include "samplor2.h"

/*VOICES*/

/*
 * samplor_init_list initializes the free list 
 */
void
samplist_init(t_samplor_list *x)
{
    t_samplor_entry *p = x->samplors;
    int i;
    
    x->free = p;
    x->used = LIST_END;
    x->at_end = LIST_END;
    for (i = 1; i < x->maxvoices; i++)
        p[i - 1].next = &p[i];
    p[--i].next = LIST_END;
}

/*
 * libere une voix
 */
t_samplor_entry
*samplist_free_voice(t_samplor_list *x, t_samplor_entry *prev, t_samplor_entry *curr)
{
    prev->next = curr->next;
    curr->next = x->free;
    x->free = curr;
    return(prev->next);
}

t_samplor_entry
*samplist_pop(t_samplor_list *x)
{
    t_samplor_entry *prev = (t_samplor_entry *)&(x->used);
    t_samplor_entry *curr = prev->next;
    t_samplor_entry *val = prev->next;
    
    if    (x->used == LIST_END)return 0;
    else
    {
        prev->next = curr->next;
        curr->next = x->free;
        x->free = curr;
        return(val);
    }
}

t_samplor_entry
*samplist_new_voice(t_samplor_list *x, long start, t_samplor_inputs inputs)
{
    long attack_dur,decay_dur,release_dur;
    long dur;
    float b_msr = 44.1;
    long long b_frames = 0;
    t_samplor_entry *new = x->free;
    
    if(new == LIST_END) return ((void*) 0);
    else
    {
        x->free = new->next;    
        
        new->buf = inputs.buf;
        new->samplenumber = inputs.samplenumber;
        new->buf_name = inputs.buf_name;
       // new->buf_obj = buffer_ref_getobject(inputs.buf_ref);
        new->start = start;
        b_msr = 44.1;
        b_frames = garray_npoints(new->buf);
        if (inputs.samplor_mmap)
        {
            new->mmap_buf = inputs.samplor_mmap;
            b_msr = new->mmap_buf->b_sr / 1000.;
            b_frames = new->mmap_buf->b_frames;
        }
        else
            new->mmap_buf = 0;
        if (inputs.samplor_buf)
        {
            new->samplor_buf = inputs.samplor_buf;
            b_msr = new->samplor_buf->b_sr / 1000.;
            b_frames = new->samplor_buf->b_frames;
        }
        else
            new->samplor_buf = 0;
        new->position = inputs.offset * b_msr;
        
        if (inputs.dur < 0)
        {
            /* loop : */
            if (inputs.dur == -2)
                new->loop_flag = ALLER_RETOUR_LOOP;
            else
                new->loop_flag = LOOP;
            
            if (inputs.susloopend)
            {   // si pas de loop dans le fichier : les points de bouclage sont donnés par le message "play" (mars 2014  : changé pour la version 2.91 à la demande de Thomas Goepfer)
                
                new->loop_beg = new->loop_beg_d = inputs.susloopstart;
                new->loop_end = new->loop_end_d = inputs.susloopend;    
            }
            /* loop point no more supported since Max6.1 API !!!*/
            
            else if (inputs.susloopstart && inputs.susloopend)
            {    //loop points
                new->loop_beg = new->loop_beg_d = inputs.susloopstart;
                new->loop_end =new->loop_end_d = inputs.susloopend;
            }
            else
            {    // si pas de loop dans le fichier : prendre tout le son (mars 2009  : changé pour la version 1.99 à la demande de Thomas Goepfer)
                // new->loop_beg = new->loop_beg_d = 256;
                // new->loop_end =new->loop_end_d = inputs.buf->b_frames  - 256;        
                new->loop_flag = IGNORE_LOOP;
            }
            new->loop_dur = new->loop_dur_d = new->loop_end - new->loop_beg;
            new->dur = b_frames;
        }
        else
        {
            /* no loop : */
            if (inputs.dur == 0)
            {
                new->loop_flag = IGNORE_LOOP;
                dur = b_frames; // joue tout le son
            }
            else
            {
                new->loop_flag = 0;
                dur = (long)(inputs.dur * b_msr);
            }
            if ((new->position + dur) > b_frames)
            /* corrige la durée si elle dépasse la fin du son */
                dur = b_frames - new->position;
            new->dur = dur;
            dur = ( long) (dur / b_msr);
            if (inputs.adsr_mode == 1 ) /* ratio mode (adsr% message)*/
            {
                inputs.attack =  0.01 *inputs.attack * dur;
                inputs.decay =  0.01 *inputs.decay * dur;
                inputs.release = 0.01 *inputs.release * dur;
            }
            else if ((inputs.attack + inputs.decay + inputs.release) > dur)
            {/* adsr correction */
                inputs.attack =  0.25 * dur;
                inputs.decay =  inputs.attack;
                inputs.release = inputs.attack;
            }
        }
        new->fposition = new->position;
        new->fposition2 = new->position;
        new->begin = new->position;
        new->end = new->begin + new->dur ;
        new->count = new->dur / inputs.transp ;
        new->increment = inputs.transp;
        new->amplitude = inputs.amp;
        new->pan = inputs.pan;
        new->chan = inputs.chan;
        new->chan2 = inputs.chan2;
        new->chan3 = inputs.chan3;
        new->chan4 = inputs.chan4;
        new->rev = inputs.rev;
        /* envelope : */
        new->win = inputs.env;
        new->sustain = inputs.sustain * 0.01;
        new->fade_out_time = 0;
        new->fade_out_end = 0;
        new->release_curve = inputs.release_curve;
        attack_dur = inputs.attack * b_msr;
        decay_dur = inputs.decay * b_msr;
        release_dur = inputs.release * b_msr;
        if (new->increment < 0) // experimental pour version 0.992 : pouvoir jouer des sons à l'envers avec une transposition négative ???
        {
            new->end = new->begin - new->dur ;
            new->count = abs ((int)new->count) ; 
            new->attack =  new->begin - attack_dur;
            new->decay =  new->attack - decay_dur;
            new->release = new->end + release_dur;            
        }
        else
        {
            new->attack =  new->begin + attack_dur;
            new->decay =  new->attack + decay_dur;
            new->release = new->end - release_dur;
        }
        new->attack_ratio = 1./(float)attack_dur;
        new->decay_ratio = (new->sustain - 1.)/(float)decay_dur;
        new->release_ratio = new->sustain/(float)release_dur;
        new->release_ratio2 = new->release_ratio / new->increment;
       
#if 0
        post ("msr frames size %f %d %d",b_msr,b_frames,garray_npoints(inputs.buf));
        post ("win st dur %d %d %d",new->win,new->start,new->dur);
        post ("pos inc amp count %f %f %f %d",new->fposition,new->increment,new->amplitude,new->count);
        post ("pan attack decay sustain dur     release %f %d %d %f %d %d",new->pan,new->attack,new->decay,new->sustain,new->dur,new->release);
        post ("loop %d %d %d",new->loop_flag,new->loop_end,new->loop_dur);
#endif
        return(new);
    }
}

t_samplor_entry
*samplist_insert(t_samplor_list *x,long start, t_samplor_inputs inputs)
{
    t_samplor_entry *pt = samplist_new_voice(x,start,inputs);
    if(pt)
    {
        if(x->used == LIST_END)
            x->at_end = pt;
        pt->next = x->used;
        x->used = pt;
    }
    return(pt);
}

t_samplor_entry
*samplist_append(t_samplor_list *x,long start, t_samplor_inputs inputs)
{
    t_samplor_entry *pt = samplist_new_voice(x,start,inputs);
    if(pt)
    {
        if(x->used == LIST_END)
            x->used = pt;
        else
            x->at_end->next = pt;
        pt->next = LIST_END;
        x->at_end = pt;
    }
    return (pt);
}

void
samplist_display(t_samplor_list *x)
{
    t_samplor_entry *pt = x->used;
     
  while (pt != LIST_END) 
     {
        post("list %d-%d ", pt->start, pt->samplenumber);
        pt=pt->next;
    }
}

/*
 * print the number of active voices
 */
int
samplist_count(t_samplor_list *x)
{
    t_samplor_entry *pt = x->used;
    int count = 0;
     
    while (pt != LIST_END) 
    {
        count ++;
        pt=pt->next;
    }
    return(count);
}

/*******************
 *  WAITING NOTES  *
 *******************/
void
list_init(t_list *x)
{
    t_list_item *p = x->items;
    int i;
    
    x->free = p;
    x->list = LIST_END;
    x->at_end = LIST_END;
    for (i = 1; i < WAITINGNOTES ; i++)
        p[i - 1].next = &p[i];
    p[--i].next = LIST_END;
}

t_list_item
*new_list_item(t_list *x,LIST_TYPE inputs)
{
  t_list_item *new = x->free;
  if(new == LIST_END)return ((void*) 0);
  else
  {
      x->free = new->next;
    new->inputs.buf_name = inputs.buf_name;
//    new->inputs.buf_ref = inputs.buf_ref ;
    new->inputs.samplor_buf = inputs.samplor_buf ;
    new->inputs.samplenumber = inputs.samplenumber;
    new->inputs.offset = inputs.offset;            
    new->inputs.dur = inputs.dur;            
    new->inputs.attack = inputs.attack;            
    new->inputs.decay = inputs.decay;            
    new->inputs.sustain = inputs.sustain;            
    new->inputs.release = inputs.release;            
    new->inputs.transp = inputs.transp;
    new->inputs.amp = inputs.amp;
    new->inputs.pan = inputs.pan;
    new->inputs.rev = inputs.rev;
    new->inputs.env = inputs.env;
    return(new);
    }
}

void
list_insert(t_list *x,LIST_TYPE inputs)
{
    t_list_item *pt = new_list_item(x,inputs);
    if(pt)
    {
        if(x->list == LIST_END)
            x->at_end = pt;
        pt->next = x->list;
        x->list = pt;
    }
}

t_list_item
*list_append(t_list *x,LIST_TYPE inputs)
{
    t_list_item *pt = new_list_item(x,inputs);
    if(pt)
    {
        if(x->list == LIST_END)
            x->list = pt;
        else
            x->at_end->next = pt;
        pt->next = LIST_END;
        x->at_end = pt;
    }
        return(pt);
}

t_list_item
*list_free(t_list *x,t_list_item *prev, t_list_item *curr)
{
    prev->next = curr->next;
    curr->next = x->free;
    x->free = curr;
    return(prev->next);    
}

t_list_item
*list_pop(t_list *x)
{
    t_list_item *prev = (t_list_item*)&(x->list);
    t_list_item *curr = prev->next;
    t_list_item *val = prev->next;
    
    if    (x->list == LIST_END)
        return 0;
    else
    {
        prev->next = curr->next;
        curr->next = x->free;
        x->free = curr;
        return(val);
    }
}

void
list_display (t_list *x)
{
    t_list_item *pt = x->list;
    post("( ");
    while (pt != LIST_END)
    {
        post("%s %f",pt->inputs.buf_name->s_name,pt->inputs.transp);
        pt=pt->next;
    }
    post(")\n");
}




