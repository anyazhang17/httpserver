#ifndef HASHMAP
#define HASHMAP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct entry entry;
typedef struct hashmap hashmap;

unsigned int hash(hashmap *hm, const char *key);
entry *entry_new(const char *key, void *value);
void entry_delete(hashmap *hm, const char *key);
hashmap *hashmap_new(int size);
void hashmap_delete(hashmap **hm);
void hashmap_add(hashmap *hm, const char *key, void *value);
void *hashmap_get(hashmap *hm, const char *key);

#endif
