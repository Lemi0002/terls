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

    for(size_t i = 0; i < atlas.indecies.count; i++) {
        size_t index = atlas.indecies.data[i].index;
        printf("%lu: %s\n", i, &atlas.characters.data[index]);
    }

    atlas_free(&atlas);

    return error_none;
}
