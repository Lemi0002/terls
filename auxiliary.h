#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define dynamic_array_fields(data_type) \
    (data_type)* data;                    \
    size_t size;                        \
    size_t count

#define dynamic_array_free(dynamic_array) free((dynamic_array)->data)

#define dynamic_array_reserve(dynamic_array, reserve) \
do { \
        while ((dynamic_array)->count + (reserve) > (dynamic_array)->size) {                                                              \
            (dynamic_array)->size = (dynamic_array)->size == 0 ? 512 : (dynamic_array)->size * 2;                           \
            (dynamic_array)->data = (typeof((dynamic_array)->data))realloc((dynamic_array)->data, (dynamic_array)->size * sizeof(*(dynamic_array)->data)); \
            assert((dynamic_array)->data != NULL);                                                                          \
        }                                                                                                                   \
} while(0)

#define dynamic_array_append(dynamic_array, value)                                                                          \
    do {                                                                                                                    \
        dynamic_array_reserve((dynamic_array), 1);               \
        (dynamic_array)->data[(dynamic_array)->count] = (value);                                                              \
        (dynamic_array)->count++;                                                                                           \
    } while (0)

// #define dynamic_array(data_type)         \
//     struct {                             \
//         dynamic_array_fields((data_type)); \
//     }
//
//


typedef struct Data {
    size_t index;
    size_t length;
} Data;

typedef struct Atlas {
    struct Indecies {
        Data* data;
        size_t size;
        size_t count;
    } indecies;
    struct Characters {
        char* data;
        size_t size;
        size_t count;
    } characters;
} Atlas;

void atlas_append(Atlas* atlas, char* string) {
    size_t length = strlen(string) + 1;
    dynamic_array_reserve(&atlas->indecies, 1);
    dynamic_array_reserve(&atlas->characters, length);

    atlas->indecies.data[atlas->indecies.count] = (Data){.index = atlas->characters.count, .length = length};
    atlas->indecies.count++;
    memcpy(atlas->characters.data + atlas->characters.count, string, length);
    atlas->characters.count += length;
}

void atlas_free(Atlas* atlas) {
    free(atlas->indecies.data);
    free(atlas->characters.data);
}

#define atlas_for_each for(size_t i = 0; i < (&atlas)->indecies.count; i++)


#endif
