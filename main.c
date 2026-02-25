#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "auxiliary.h"

enum Error {
    error_none = 0,
    error_opendir,
    error_malloc,
    error_readdir
};

int main(int argc, char** argv) {
    DIR* directory_stream = opendir("./");
    if (directory_stream == NULL) {
        return error_opendir;
    }

    errno = 0;
    Atlas atlas = {0};
    struct dirent* entry = readdir(directory_stream);

    while (entry != NULL) {
        atlas_append(&atlas, entry->d_name);
        entry = readdir(directory_stream);
    }

    if (errno != 0) {
        return error_readdir;
    }

    printf("%lu\n", atlas.indecies.size);
    printf("%lu\n", atlas.indecies.count);
    printf("%lu\n", atlas.characters.size);
    printf("%lu\n", atlas.characters.count);

    for(size_t i = 0; i < atlas.indecies.count; i++) {
        printf("%lu: index:%lu length:%lu\n", i, atlas.indecies.data[i].index, atlas.indecies.data[i].length);
    }

    printf("Raw:\n");
    for(size_t i = 0; i < atlas.indecies.count; i++) {
        printf("%lu: %s\n", i, atlas_get_string_at_index(&atlas, i));
    }

    size_t *sort = malloc(sizeof(sort) * atlas.indecies.count);
    for(size_t i = 0; i < atlas.indecies.count; i++) {
        sort[i] = i;
    }

    for(size_t i = 0; i < atlas.indecies.count; i++) {
        for(size_t j = i + 1; j < atlas.indecies.count; j++) {
            int result = strcmp(atlas_get_string_at_index(&atlas, sort[i]), atlas_get_string_at_index(&atlas, sort[j]));
            if (result > 0) {
                size_t tempo = sort[i];
                sort[i] = sort[j];
                sort[j] = tempo;
            }
        }
    }

    printf("Sorted:\n");
    for(size_t i = 0; i < atlas.indecies.count; i++) {
        printf("%lu: %s\n", i, atlas_get_string_at_index(&atlas, sort[i]));
    }

    atlas_free(&atlas);
    free(sort);

    return error_none;
}
