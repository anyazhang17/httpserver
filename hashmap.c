#include "hashmap.h"
#include <assert.h>

struct entry {
    const char *key;
    void *value;
    entry *next;
};

struct hashmap {
    entry **buckets;
    int size;
    int current_size;
};

//hash function
unsigned int hash(hashmap *hm, const char *key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash * 31) + *key++;
    }
    return hash % hm->size;
}

//creates entry ADT of key value pair
entry *entry_new(const char *key, void *value) {
    entry *e = malloc(sizeof(entry));
    if (e == NULL) {
        return NULL;
    }
    e->key = strdup(key); // Make a copy of the key
    e->value = value;
    e->next = NULL;
    return e;
}

//removes entry from hashmap
void entry_delete(hashmap *hm, const char *key) {
    unsigned int index = hash(hm, key);
    entry *current = hm->buckets[index];
    entry *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                hm->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

//creates a hashmap ADT
hashmap *hashmap_new(int size) {
    hashmap *hm = malloc(sizeof(hashmap));
    if (hm == NULL) {
        return NULL;
    }
    hm->buckets = calloc(size, sizeof(entry *));
    if (hm->buckets == NULL) {
        free(hm);
        return NULL;
    }
    hm->size = size;
    hm->current_size = 0;
    return hm;
}

//deallocates memory for hashmap
void hashmap_delete(hashmap **hm) {
    if (hm != NULL && *hm != NULL) {
        for (int i = 0; i < (*hm)->size; i++) {
            entry *current = (*hm)->buckets[i];
            while (current != NULL) {
                entry *temp = current;
                current = current->next;
                entry_delete(*hm, temp->key);
            }
        }
        free((*hm)->buckets);
        free(*hm);
    }
    *hm = NULL;
}
//might be mapping diff uris to same rwlock/same uris to diff rwlock
//adds key value to hashmap
void hashmap_add(hashmap *hm, const char *key, void *value) {
    unsigned int index = hash(hm, key);
    entry *e = entry_new(key, value);
    e->next = hm->buckets[index];
    hm->buckets[index] = e;
    hm->current_size++;
}

//gets the value of the key from the hashmap
void *hashmap_get(hashmap *hm, const char *key) {
    //printf("hashmap current size is = %d\n", hm->current_size);
    unsigned int index = hash(hm, key);
    //printf("hashed index = %d\n", index);
    entry *e = hm->buckets[index];
    while (e != NULL) {
        //printf("e->key: %s & key: %s\n", e->key, key);
        if (strcmp(e->key, key) == 0) {
            return e->value;
        }
        e = e->next;
        //printf("e->next THIS SHOULD NOT PRINT!!!!!!\n");
    }
    return NULL;
}

// int main(void) {
//     hashmap *h = hashmap_new(3);
//     char* s1 = "hello\n";
//     char* s2 = "hi\n";
//     hashmap_add(h, "filename1", s1);
//     hashmap_add(h, "filename2", s2);

//     void* val = hashmap_get(h, "filename1");
//     assert(strcmp(s1, val) == 0);
//     val = hashmap_get(h, "filename2");
//     assert(strcmp(s2, val) == 0);
//     val = hashmap_get(h, "filename3");
//     assert(val == NULL);
//     for (int i = 0; i < 100; i++) {
//         void* val = hashmap_get(h, "uri");
//         if (val == NULL) {
//             hashmap_add(h, "uri", s1);
//             printf("added uri!\n");
//         }
//     }

//     hashmap_delete(&h);
//     printf("passed!\n");
//     return 0;
// }
