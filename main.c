#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>

#include "auxiliary.h"

typedef enum Error {
    error_none = 0,
    error_opendir,
    error_malloc,
    error_readdir,
    error_stat,
    error_setvbuf,
    error_tcgetattr,
    error_tcsetattr,
    error_read,
    error_tui_check,
    error_tui_compile,
} Error;

typedef struct Entry {
    ino_t inode;
    mode_t mode;
    nlink_t link_count;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t time_modified;
} Entry;

typedef struct Entries {
    Entry* data;
    size_t size;
    size_t count;
} Entries;

char entry_convert_type_to_character(Entry *entry) {
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

const char* entry_convert_type_to_string(Entry *entry) {
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

enum {
    MODE_PERMISSIONS_WIDTH = 9,
    MODE_PERMISSIONS_MASK = (1 << MODE_PERMISSIONS_WIDTH) - 1
};

typedef struct Mode {
    char data[MODE_PERMISSIONS_WIDTH + 1];
} Mode;

Mode entry_convert_permissions_to_string(Entry *entry) {
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

Time entry_convert_time_modified_to_string(Entry *entry) {
    Time time;
    int data;
    size_t index = 0;
    struct tm* time_local = localtime(&entry->time_modified);

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

typedef enum Sort {
    SORT_NONE,
    SORT_INODE,
    SORT_TYPE,
    SORT_PERMISSIONS,
    SORT_LINK_COUNT,
    SORT_UID,
    SORT_GID,
    SORT_SIZE,
    SORT_TIME_MODIFIED,
    SORT_NAME
} Sort;

#define bubble_sort_lambda(map, count, lambda) \
    do { \
        for(size_t i = 0; i < (count); i++) { \
            for(size_t j = i + 1; j < (count); j++) { \
                int result = (lambda); \
                if (result > 0) { \
                    size_t tempo = (map)[i]; \
                    (map)[i] = (map)[j]; \
                    (map)[j] = tempo; \
                } \
            } \
        } \
    } while(0)

void entry_create_sorted_map(Entries *const entries, Atlas *const atlas, Sort sort, size_t *const map) {
    for(size_t i = 0; i < atlas->indecies.count; i++) {
        map[i] = i;
    }

    switch(sort) {
        case SORT_NONE:
            break;

        case SORT_INODE:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].inode > entries->data[map[j]].inode ? 1 : 0);
            break;

        case SORT_TYPE:
            bubble_sort_lambda(map, entries->count, entry_convert_type_to_character(&entries->data[map[i]]) > entry_convert_type_to_character(&entries->data[map[j]]) ? 1 : 0);
            break;

        case SORT_PERMISSIONS:
            bubble_sort_lambda(map, entries->count, (entries->data[map[i]].mode & MODE_PERMISSIONS_MASK) > (entries->data[map[j]].mode & MODE_PERMISSIONS_MASK) ? 1 : 0);
            break;

        case SORT_LINK_COUNT:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].link_count > entries->data[map[j]].link_count ? 1 : 0);
            break;

        case SORT_UID:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].uid > entries->data[map[j]].uid ? 1 : 0);
            break;

        case SORT_GID:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].gid > entries->data[map[j]].gid ? 1 : 0);
            break;

        case SORT_SIZE:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].size > entries->data[map[j]].size ? 1 : 0);
            break;

        case SORT_TIME_MODIFIED:
            bubble_sort_lambda(map, entries->count, entries->data[map[i]].time_modified > entries->data[map[j]].time_modified ? 1 : 0);
            break;

        case SORT_NAME:
            bubble_sort_lambda(map, entries->count, strcmp(atlas_get_string_at_index(atlas, map[i]), atlas_get_string_at_index(atlas, map[j])));
            break;

        default: break;
    }
}

#define integer_max_lambda(count, lambda, result) \
    do { \
        size_t max = 0; \
        for(size_t i = 0; i < (count); i++) { \
            if ((lambda) > max ) { \
                max = (lambda); \
            } \
        } \
        *result = max; \
    } while(0)

int width_integer(size_t value) {
    int width = 0;
    do {
        value /= 10;
        width++;
    } while(value);
    return width;
}

size_t width_string(const char* string) {
    return strlen(string);
}

Error terminal_apply_settings(struct termios* settings_saved) {
    printf(
        ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER
        ASCII_CURSOR_MOVE_TO_HOME
    );

    if(setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        return error_setvbuf;
    }

    struct termios settings;
    if (tcgetattr(STDIN_FILENO, &settings) < 0) {
        return error_tcgetattr;
    }

    *settings_saved = settings;
    settings.c_lflag &= ~(ICANON | ECHO);
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) < 0) {
        return error_tcsetattr;
    }

    return error_none;
}

Error terminal_restore_settings(struct termios* settings_saved) {
    if (tcsetattr(STDIN_FILENO, TCSANOW, settings_saved) < 0) {
        return error_tcsetattr;
    }

    printf(
        ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER
        ASCII_RESET
    );

    return error_none;
}

volatile sig_atomic_t terminal_size_updated;
volatile struct winsize terminal_size;

void signal_callback(int signal) {
    switch (signal) {
        case SIGWINCH:
            terminal_size_updated = 1;
            ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);
            break;
    }
}

Error entry_read_directory(char* directory, Atlas* entry_names, Entries* entries) {
    DIR* directory_stream = opendir(directory);
    if (directory_stream == NULL) {
        return error_opendir;
    }

    struct dirent* entry = readdir(directory_stream);

    while (entry != NULL) {
        atlas_append(entry_names, entry->d_name);
        entry = readdir(directory_stream);
    }

    if (errno != 0) {
        return error_readdir;
    }

    if (entry_names->indecies.count == 0) {
        return error_none;
    }

    String_Builder path = {0};

    for(size_t i = 0; i < entry_names->indecies.count; i++) {
        struct stat stats;
        string_builder_reset(&path);
        string_builder_append_string(&path, directory);
        if (path.data[path.count - 1] != '/') {
            string_builder_append_character(&path, '/');
        }
        string_builder_append_string(&path, atlas_get_string_at_index(entry_names, i));
        string_builder_append_character(&path, 0);

        if (lstat(path.data, &stats) < 0) {
            return error_stat;
        }

        dynamic_array_append(entries, ((Entry){
            .inode = stats.st_ino,
            .mode = stats.st_mode,
            .link_count = stats.st_nlink,
            .uid = stats.st_uid,
            .gid = stats.st_gid,
            .size = stats.st_size,
            .time_modified = stats.st_mtime
        }));
    }

    string_builder_free(&path);
    return error_none;
}

void entry_print(Atlas* entry_names, Entries* entries, size_t* map) {
    int index_width = width_integer(entries->count);
    int inode_width;
    int link_count_width;
    int size_width;
    int uid_width;
    int gid_width;

    integer_max_lambda(entries->count, width_integer(entries->data[i].inode), &inode_width);
    integer_max_lambda(entries->count, width_integer(entries->data[i].link_count), &link_count_width);
    integer_max_lambda(entries->count, width_integer(entries->data[i].size), &size_width);
    integer_max_lambda(entries->count, width_integer(entries->data[i].uid), &uid_width);
    integer_max_lambda(entries->count, width_integer(entries->data[i].gid), &gid_width);
    // integer_max_lambda(entries.count, width_string(getpwuid(entries.data[i].uid)->pw_name), &uid_width);
    // integer_max_lambda(entries.count, width_string(getgrgid(entries.data[i].gid)->gr_name), &gid_width);

    for(size_t i = 0; i < entries->count; i++) {
        printf(
            ASCII_MODE_ENABLE_DIM"%*lu:"ASCII_MODE_DISABLE_DIM" %c%s"" | %-*lu"" | %-*lu"" | %-*d"" | %-*d"" | %*lu"" | %s"" | %s\n",
            index_width, i + 1,
            entry_convert_type_to_character(&entries->data[map[i]]), entry_convert_permissions_to_string(&entries->data[map[i]]).data,
            inode_width, entries->data[map[i]].inode,
            link_count_width, entries->data[map[i]].link_count,
            uid_width, entries->data[map[i]].uid,
            gid_width, entries->data[map[i]].gid,
            size_width, entries->data[map[i]].size,
            entry_convert_time_modified_to_string(&entries->data[map[i]]).data,
            atlas_get_string_at_index(entry_names, map[i])
        );
    }
}

int main(int argc, char** argv) {
    char* directory;

    if (argc == 1) {
        directory = "./";
    } else {
        directory = argv[1];
    }

    errno = 0;
    Error error;
    Atlas entry_names = {0};
    Entries entries = {0};

    error = entry_read_directory(directory, &entry_names, &entries);
    if (error != error_none) {
        return error;
    }

    size_t *map = malloc(sizeof(map) * entries.count);
    entry_create_sorted_map(&entries, &entry_names, SORT_NAME, map);

    struct termios settings_saved;
    error = terminal_apply_settings(&settings_saved);
    if (error != error_none) {
        return error;
    }

    terminal_size_updated = 1;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);
    signal(SIGWINCH, signal_callback);

    Tui_Window windows[] = {
        tui_window_make_root(0, TUI_LAYOUT_VERTICAL,
            tui_window_make_text(0, 1, tui_size_make_fixed(1)),
            tui_window_make_layout(0, 2, tui_size_make_fill(), TUI_LAYOUT_HORIZONTAL,
                tui_window_make_text(2, 3, tui_size_make_ratio(0.5)),
                tui_window_make_text(2, 4, tui_size_make_fixed(1)),
                tui_window_make_text(2, 5, tui_size_make_fill())),
            tui_window_make_text(0, 6, tui_size_make_fixed(1))
        )
    };
    Tui_Window* scratchpad[general_array_size(windows)];

    Tui_Error tui_error = tui_check(windows, scratchpad, general_array_size(windows));
    if(tui_error != TUI_ERROR_NONE) {
        return error_tui_check;
    }

    char input = 0;
    do {
        if (terminal_size_updated) {
            terminal_size_updated = 0;
            printf(
                ASCII_ERASE_SCREEN
            );

            Tui_Bounding_Box bounding_box = {.x = 1, .y = 1, .width = terminal_size.ws_col, .height = terminal_size.ws_row};
            tui_error = tui_update(windows, scratchpad, general_array_size(windows), &bounding_box);
            if(tui_error != TUI_ERROR_NONE) {
                return error_tui_compile;
            }

            for(size_t i = 0; i < general_array_size(windows); i++) {
                if(windows[i].id == 3) {
                    printf(ASCII_CURSOR_MOVE_TO_POSITION, windows[i].bounding_box.y, windows[i].bounding_box.x);
                    entry_print(&entry_names, &entries, map);
                } else if (windows[i].id == 4) {
                    printf(ASCII_CURSOR_MOVE_TO_POSITION, windows[i].bounding_box.y, windows[i].bounding_box.x);
                    for(uint32_t dy = 0; dy < windows[i].bounding_box.height; dy++) {
                        printf("|" ASCII_CURSOR_MOVE_DOWN ASCII_CURSOR_MOVE_LEFT, 1, 1);
                    }
                }
            }
        }

        ssize_t count = read(STDIN_FILENO, &input, 1);
        if(count < 0) {
            return error_read;
        } else if(count > 0) {
            switch(input) {
                case 'q': break;
                case 'j': printf(ASCII_CURSOR_MOVE_DOWN, 1); break;
                case 'k': printf(ASCII_CURSOR_MOVE_UP, 1); break;
                case 'h': printf(ASCII_CURSOR_MOVE_LEFT, 1); break;
                case 'l': printf(ASCII_CURSOR_MOVE_RIGHT, 1); break;
                default: break;
            }
        }

        usleep(5000);
    } while (input != 'q');

    error = terminal_restore_settings(&settings_saved);
    if (error != error_none) {
        return error;
    }

    free(map);
    atlas_free(&entry_names);
    dynamic_array_free(&entries);

    return error_none;
}
