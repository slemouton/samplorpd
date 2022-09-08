//
//  hashtable.h
//  samplorpd~
//
//  Created by serge lemouton on 29/04/2020.
//  Copyright © 2020 Serge Lemouton. All rights reserved.
//

#ifndef hashtable_h
#define hashtable_h

#include <stdio.h>
#include "max_types.h"

struct hash_table_entry
{
    char *key;
    char *value;
    t_int64 val1;      /*to store loop points in samples*/
    t_int64 val2;
    struct hash_table_entry *next;
};

struct hash_table 
{
    size_t size;
    struct hash_table_entry **table;
};
struct hash_table* init_hash_table(size_t number_of_rows);
int ht_insert(struct hash_table *the_hash_table, char *key, char *value);
int ht_insert2(struct hash_table *the_hash_table, char *key, t_int64 v1, t_int64 v2);
int ht_find(struct hash_table *the_hash_table, char *key, struct hash_table_entry **ret_val);
int ht_delete(struct hash_table *the_hash_table, char *key);
void ht_deleteTable(struct hash_table *my_table);

#endif /* hashtable_h */
