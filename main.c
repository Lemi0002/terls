#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "auxiliary.h"

enum Error {
    error_none = 0,
    error_opendir,
    error_malloc,
    error_readdir,
    error_stat,
};

typedef struct Entry {
    ino_t inode;
    mode_t mode;
    nlink_t link_count;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t time_mofified;
} Entry;

typedef struct Entries {
    Entry* data;
    size_t size;
    size_t count;
} Entries;

char entry_map_type_to_character(Entry *entry) {
    char value;
    switch(entry->mode & S_IFMT) {
        case S_IFBLK: value = 'b'; break;
        case S_IFCHR: value = 'c'; break;
        case S_IFDIR: value = 'd'; break;
        case S_IFIFO: value = 'f'; break;
        case S_IFLNK: value = 'l'; break;
        case S_IFREG: value = '-'; break;
        case S_IFSOCK: value = 's'; break;
        default: value = '?'; break;
    }
    return value;
}

const char* entry_map_type_to_string(Entry *entry) {
    const char* string;
    switch(entry->mode & S_IFMT) {
        case S_IFBLK: string = "block device"; break;
        case S_IFCHR: string = "character device"; break;
        case S_IFDIR: string = "directory"; break;
        case S_IFIFO: string = "named pipe"; break;
        case S_IFLNK: string = "symbolic link"; break;
        case S_IFREG: string = "regular file"; break;
        case S_IFSOCK: string = "socket"; break;
        default: string = "unknown"; break;
    }
    return string;
}

typedef struct Mode {
    char data[10];
} Mode;

Mode entry_map_permissions_to_string(Entry *entry) {
    Mode mode;
    for(size_t i = 0; i < sizeof(mode) - 1; ++i) {
        if (entry->mode & (1 << (sizeof(mode) - 2 - i))) {
            switch(i % 3) {
                case 0: mode.data[i] = 'r'; break;
                case 1: mode.data[i] = 'w'; break;
                case 2: mode.data[i] = 'x'; break;
            }
        } else {
            mode.data[i] = '-';
        }
    }

    mode.data[sizeof(mode) - 1] = 0;
    return mode;
}

typedef struct Time {
    char data[17];
} Time;

size_t convert(char* buffer, int data, int width, size_t start) {
    for(size_t i = width; i > 0; --i) {
        buffer[start + i - 1] = '0' + (data % 10);
        data = data / 10;
    }
    return start + width;
}

Time entry_map_time_modified_to_string(Entry *entry) {
    Time time;
    int data;
    size_t index = 0;
    struct tm* time_local = localtime(&entry->time_mofified);

    index = convert(time.data, time_local->tm_year + 1900, 4, index);
    time.data[index++] = '-';
    index = convert(time.data, time_local->tm_mon, 2, index);
    time.data[index++] = '-';
    index = convert(time.data, time_local->tm_mday, 2, index);
    time.data[index++] = ' ';
    index = convert(time.data, time_local->tm_hour, 2, index);
    time.data[index++] = ':';
    index = convert(time.data, time_local->tm_min, 2, index);
    time.data[index++] = 0;
    return time;
}

int main(int argc, char** argv) {
    DIR* directory_stream = opendir("./");
    if (directory_stream == NULL) {
        return error_opendir;
    }

    errno = 0;
    Atlas atlas = {0};
    Entries entries = {0};
    struct dirent* entry = readdir(directory_stream);

    while (entry != NULL) {
        atlas_append(&atlas, entry->d_name);
        entry = readdir(directory_stream);
    }

    if (errno != 0) {
        return error_readdir;
    }

    for(size_t i = 0; i < atlas.indecies.count; i++) {
        struct stat stats;
        if (stat(atlas_get_string_at_index(&atlas, i), &stats) < 0) {
            return error_stat;
        }

        dynamic_array_append(&entries, ((Entry){
            .inode = stats.st_ino,
            .mode = stats.st_mode,
            .link_count = stats.st_nlink,
            .uid = stats.st_uid,
            .gid = stats.st_gid,
            .size = stats.st_size,
            .time_mofified = stats.st_mtime
        }));
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

    // printf("ls:\n");
    // for(size_t i = 0; i < entries.count; i++) {
    //     printf("%lu:", i);
    //     printf(" %c", entry_map_type_to_character(&entries.data[sort[i]]));
    //     printf("%s", entry_map_permissions_to_string(&entries.data[sort[i]]).data);
    //     printf(" %-8lu", entries.data[sort[i]].link_count);
    //     printf("%-10s", getpwuid(entries.data[sort[i]].uid)->pw_name);
    //     printf(" %-10s", getgrgid(entries.data[sort[i]].gid)->gr_name);
    //     printf(" %10lu", entries.data[sort[i]].size);
    //     printf(" %s", entry_map_time_modified_to_string(&entries.data[sort[i]]).data);
    //     printf(" %s\n", atlas_get_string_at_index(&atlas, sort[i]));
    // }

    for(size_t i = 0; i < entries.count; i++) {
        printf("%lu:", i);
        printf(" %c%s", entry_map_type_to_character(&entries.data[sort[i]]), entry_map_permissions_to_string(&entries.data[sort[i]]).data);
        printf(" | %-4lu", entries.data[sort[i]].link_count);
        printf(" | %-10s", getpwuid(entries.data[sort[i]].uid)->pw_name);
        printf(" | %-10s", getgrgid(entries.data[sort[i]].gid)->gr_name);
        printf(" | %10lu", entries.data[sort[i]].size);
        printf(" | %s", entry_map_time_modified_to_string(&entries.data[sort[i]]).data);
        printf(" | %s\n", atlas_get_string_at_index(&atlas, sort[i]));
    }

    atlas_free(&atlas);
    dynamic_array_free(&entries);
    free(sort);

    return error_none;
}
