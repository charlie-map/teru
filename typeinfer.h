#ifndef __TYPEINFER_L__
#define __TYPEINFER_L__

#include "hashmap.h"

hashmap *infer_load();

char *content_type_infer(hashmap *load_map, char *filename, char *data, int data_length);
int is_binary(hashmap *load_map, char *filename);

#endif