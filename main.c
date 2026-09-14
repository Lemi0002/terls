#include <linux/limits.h>
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
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include "auxiliary.h"

typedef enum Error {
    error_none = 0,
    error_realpath,
    error_opendir,
    error_malloc,
    error_readdir,
    error_stat,
    error_getcwd,
    error_setvbuf,
    error_tcgetattr,
    error_tcsetattr,
    error_read,
    error_tui_check,
    error_tui_update,
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

typedef struct Map {
    size_t *data;
    size_t size;
    size_t count;
} Map;

typedef struct Record {
    Entries entries;
    Atlas names;
    Map map;
} Record;

void record_reset(Record *record) {
    atlas_reset(&record->names);
    dynamic_array_reset(&record->entries);
    dynamic_array_reset(&record->map);
}

void record_free(Record *record) {
    atlas_free(&record->names);
    dynamic_array_free(&record->entries);
    dynamic_array_free(&record->map);
}

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

void entry_append_permissions(Entry *entry, String_Builder* string_builder) {
    const size_t width = 9;
    for(size_t i = 0; i < width; ++i) {
        if (entry->mode & (1 << (width - 1 - i))) {
            switch(i % 3) {
                case 0: string_builder_append_character(string_builder, 'r'); break;
                case 1: string_builder_append_character(string_builder, 'w'); break;
                case 2: string_builder_append_character(string_builder, 'x'); break;
            }
        } else {
            string_builder_append_character(string_builder, '-');
        }
    }
}

void entry_append_time_modified(Entry *entry, String_Builder* string_builder) {
    struct tm* time_local = localtime(&entry->time_modified);

    string_builder_append_integer(string_builder, time_local->tm_year + 1900, 4, '0');
    string_builder_append_character(string_builder, '-');
    string_builder_append_integer(string_builder, time_local->tm_mon, 2, '0');
    string_builder_append_character(string_builder, '-');
    string_builder_append_integer(string_builder, time_local->tm_mday, 2, '0');
    string_builder_append_character(string_builder, ' ');
    string_builder_append_integer(string_builder, time_local->tm_hour, 2, '0');
    string_builder_append_character(string_builder, ':');
    string_builder_append_integer(string_builder, time_local->tm_min, 2, '0');
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

void record_create_sorted_map(Record *record, Sort sort) {
    dynamic_array_reset(&record->map);
    for(size_t i = 0; i < record->names.indecies.count; i++) {
        dynamic_array_append(&record->map, i);
    }

    switch(sort) {
        case SORT_NONE:
            break;

        case SORT_INODE:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].inode > record->entries.data[record->map.data[j]].inode ? 1 : 0
            );
            break;

        case SORT_TYPE:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                entry_convert_type_to_character(&record->entries.data[record->map.data[i]]) > entry_convert_type_to_character(&record->entries.data[record->map.data[j]]) ? 1 : 0
            );
            break;

        case SORT_PERMISSIONS:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                (record->entries.data[record->map.data[i]].mode & MODE_PERMISSIONS_MASK) > (record->entries.data[record->map.data[j]].mode & MODE_PERMISSIONS_MASK) ? 1 : 0
            );
            break;

        case SORT_LINK_COUNT:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].link_count > record->entries.data[record->map.data[j]].link_count ? 1 : 0
            );
            break;

        case SORT_UID:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].uid > record->entries.data[record->map.data[j]].uid ? 1 : 0
            );
            break;

        case SORT_GID:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].gid > record->entries.data[record->map.data[j]].gid ? 1 : 0
            );
            break;

        case SORT_SIZE:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].size > record->entries.data[record->map.data[j]].size ? 1 : 0
            );
            break;

        case SORT_TIME_MODIFIED:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                record->entries.data[record->map.data[i]].time_modified > record->entries.data[record->map.data[j]].time_modified ? 1 : 0
            );
            break;

        case SORT_NAME:
            bubble_sort_lambda(
                record->map.data,
                record->entries.count,
                strcmp(atlas_get_cstring_at_index(&record->names, record->map.data[i]), atlas_get_cstring_at_index(&record->names, record->map.data[j]))
            );
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

Error record_read_directory(Record *record, String directory) {
    DIR* directory_stream = opendir(directory.cstring);
    if (directory_stream == NULL) {
        return error_opendir;
    }

    errno = 0;
    struct dirent* entry = readdir(directory_stream);

    while (entry != NULL) {
        atlas_append_cstring(&record->names, entry->d_name);
        entry = readdir(directory_stream);
    }

    if (errno != 0) {
        return error_readdir;
    }

    if (record->names.indecies.count == 0) {
        return error_none;
    }

    String_Builder path = {0};

    for(size_t i = 0; i < record->names.indecies.count; i++) {
        struct stat stats;
        string_builder_reset(&path);
        string_builder_append_string(&path, directory);
        if (path.data[path.count - 1] != '/') {
            string_builder_append_character(&path, '/');
        }
        string_builder_append_string(&path, atlas_get_string_at_index(&record->names, i));
        string_builder_append_character(&path, 0);

        if (lstat(path.data, &stats) < 0) {
            string_builder_free(&path);
            return error_stat;
        }

        dynamic_array_append(&record->entries, ((Entry){
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

void record_convert_to_atlas(Record *record, Atlas* atlas) {
    uint8_t index_width = general_integer_width(record->entries.count);
    uint8_t inode_width;
    uint8_t link_count_width;
    uint8_t size_width;
    uint8_t uid_width;
    uint8_t gid_width;

    integer_max_lambda(record->entries.count, general_integer_width(record->entries.data[i].inode), &inode_width);
    integer_max_lambda(record->entries.count, general_integer_width(record->entries.data[i].link_count), &link_count_width);
    integer_max_lambda(record->entries.count, general_integer_width(record->entries.data[i].size), &size_width);
    integer_max_lambda(record->entries.count, general_integer_width(record->entries.data[i].uid), &uid_width);
    integer_max_lambda(record->entries.count, general_integer_width(record->entries.data[i].gid), &gid_width);

    for(size_t i = 0; i < record->entries.count; i++) {
        String_Builder* string_builder = atlas_string_builder_begin(atlas);
        string_builder_append_character(string_builder, entry_convert_type_to_character(&record->entries.data[record->map.data[i]]));
        entry_append_permissions(&record->entries.data[record->map.data[i]], string_builder);
        string_builder_append_character(string_builder, ' ');
        string_builder_append_integer(string_builder, record->entries.data[record->map.data[i]].inode, inode_width, ' ');
        string_builder_append_character(string_builder, ' ');
        string_builder_append_integer(string_builder, record->entries.data[record->map.data[i]].link_count, link_count_width, ' ');
        string_builder_append_character(string_builder, ' ');
        string_builder_append_integer(string_builder, record->entries.data[record->map.data[i]].uid, uid_width, ' ');
        string_builder_append_character(string_builder, ' ');
        string_builder_append_integer(string_builder, record->entries.data[record->map.data[i]].gid, gid_width, ' ');
        string_builder_append_character(string_builder, ' ');
        string_builder_append_integer(string_builder, record->entries.data[record->map.data[i]].size, size_width, ' ');
        string_builder_append_character(string_builder, ' ');
        entry_append_time_modified(&record->entries.data[record->map.data[i]], string_builder);
        string_builder_append_character(string_builder, ' ');
        string_builder_append_string(string_builder, atlas_get_string_at_index(&record->names, record->map.data[i]));
        atlas_string_builder_end(atlas);
    }
}

Error app_update_primary(Record *record_main, Atlas *record_main_atlas, String_Builder *directory) {
    record_reset(record_main);
    atlas_reset(record_main_atlas);

    Error error = record_read_directory(record_main, string_from_cstring(directory->data));
    if (error != error_none) {
        return error;
    }

    record_create_sorted_map(record_main, SORT_NAME);
    record_convert_to_atlas(record_main, record_main_atlas);

    return error_none;
}

Error app_update_primary_line_number(Record *primary_record, Atlas *primary_line_number) {
    const size_t index_width = 4;

    atlas_reset(primary_line_number);

    for(size_t i = 0; i < primary_record->entries.count; i++) {
        String_Builder* string_builder = atlas_string_builder_begin(primary_line_number);
        // string_builder_append_cstring(string_builder, ASCII_MODE_ENABLE_DIM);
        string_builder_append_integer(string_builder, i + 1, index_width, ' ');
        // string_builder_append_cstring(string_builder, ":" ASCII_MODE_DISABLE_DIM " ");
        string_builder_append_cstring(string_builder, ": ");
        atlas_string_builder_end(primary_line_number);
    }

    return error_none;
}

Error app_update_secondary(Record *record_main, Tui_Element_Scrollable *scrollable_main, Record *record_preview, Atlas *record_preview_atlas, String_Builder *directory) {
    const size_t index = record_main->map.data[scrollable_main->selection];
    Entry *const entry = &record_main->entries.data[index];
    const String entry_name = atlas_get_string_at_index(&record_main->names, index);

    if (entry_convert_type_to_character(entry) == 'd') {
        String_Builder path = {0};

        record_reset(record_preview);
        atlas_reset(record_preview_atlas);

        string_builder_append_string(&path, string_from_cstring(directory->data));
        if (path.data[path.count - 1] != '/') {
            string_builder_append_character(&path, '/');
        }
        string_builder_append_string(&path, entry_name);
        string_builder_append_character(&path, 0);

        Error error = record_read_directory(record_preview, string_from_cstring(path.data));
        if (error != error_none) {
            return error;
        }

        record_create_sorted_map(record_preview, SORT_NAME);
        record_convert_to_atlas(record_preview, record_preview_atlas);
    }

    return error_none;
}

Error app_path_up(String_Builder *path) {
    const char root[] = "/";

    assert(path->size >= general_array_size(root));
    assert(path->count >= general_array_size(root));
    assert(path->data[0] == '/');
    assert(path->data[path->count - 1] == 0);

    if(memcmp(path->data, root, general_array_size(root)) == 0) {
        return error_none;
    }

    assert(path->count >= general_array_size(root) + 1);
    assert(path->data[path->count - 2] != '/');

    ssize_t index;
    for(index = path->count - 2; index >= 0; --index) {
        if(path->data[index] == '/') {
            break;
        }
    }

    if (index == 0) {
        path->data[index + 1] = 0;
        path->count = index + 2;
    } else {
        path->data[index] = 0;
        path->count = index + 1;
    }

    return error_none;
}

Error app_path_down(String_Builder *path, Record *record, Tui_Element_Scrollable *scrollable) {
    const size_t index = record->map.data[scrollable->selection];
    Entry *const entry = &record->entries.data[index];
    const String entry_name = atlas_get_string_at_index(&record->names, index);
    const char root[] = "/";

    assert(path->count >= general_array_size(root));
    assert(path->data[0] == '/');
    assert(path->data[path->count - 1] == 0);

    if (entry_convert_type_to_character(entry) == 'd') {
        if (path->data[path->count - 2] != '/') {
            path->data[path->count - 1] = '/';
        } else {
            path->count--;
        }
        string_builder_append_string(path, entry_name);
        string_builder_append_character(path, 0);
    }

    return error_none;
}

int main(int argc, char** argv) {
    String_Builder directory = {0};
    string_builder_reserve(&directory, PATH_MAX);
    const char *directory_input;

    if (argc == 1) {
        directory_input = ".";
    } else {
        directory_input = argv[1];
    }

    if (realpath(directory_input, directory.data) == NULL) {
        return error_realpath;
    }
    directory.count = strlen(directory.data) + 1;

    Error error;
    Record primary_record = {0};
    Record secondary_record = {0};
    Atlas primary_line_number = {0};
    Atlas primary_atlas = {0};
    Atlas secondary_atlas = {0};

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
            tui_window_make_leaf(0, 1, tui_size_make_fixed(1)),
            tui_window_make_layout(0, 2, tui_size_make_fill(), TUI_LAYOUT_HORIZONTAL,
                tui_window_make_leaf(2, 3, tui_size_make_fixed(6)),
                tui_window_make_leaf(2, 4, tui_size_make_ratio(0.5)),
                tui_window_make_leaf(2, 5, tui_size_make_fixed(1)),
                tui_window_make_leaf(2, 6, tui_size_make_fill())),
            tui_window_make_leaf(0, 7, tui_size_make_fixed(1))
        )
    };
    Tui_Window* windows_scratchpad[general_array_size(windows)];

    Tui_Element_Text text_path = {0};
    text_path.window = &windows[1];

    Tui_Element_Scrollable scrollable_primary_line_number = {0};
    scrollable_primary_line_number.window = &windows[3];
    scrollable_primary_line_number.atlas = &primary_line_number;
    scrollable_primary_line_number.selection_format = (String){.cstring = ASCII_MODE_ENABLE_UNDERLINE, .length = ASCII_MODE_ENABLE_UNDERLINE_LENGTH};

    Tui_Element_Scrollable scrollable_primary = {0};
    scrollable_primary.window = &windows[4];
    scrollable_primary.atlas = &primary_atlas;
    scrollable_primary.selection_format = (String){.cstring = ASCII_MODE_ENABLE_UNDERLINE, .length = ASCII_MODE_ENABLE_UNDERLINE_LENGTH};

    Tui_Element_Line line = {0};
    line.window = &windows[5];

    Tui_Element_Scrollable scrollable_secondary = {0};
    scrollable_secondary.window = &windows[6];
    scrollable_secondary.atlas = &secondary_atlas;
    scrollable_secondary.selection_format = (String){.cstring = "", .length = 0};

    Tui_Error tui_error = tui_check(windows, windows_scratchpad, general_array_size(windows));
    if(tui_error != TUI_ERROR_NONE) {
        return error_tui_check;
    }

    char input = 0;
    do {
        if (terminal_size_updated) {
            terminal_size_updated = 0;
            write(STDOUT_FILENO, ASCII_ERASE_SCREEN, ASCII_ERASE_SCREEN_LENGTH);

            Tui_Bounding_Box bounding_box = {.x = 1, .y = 1, .width = terminal_size.ws_col, .height = terminal_size.ws_row};
            tui_update(windows, windows_scratchpad, general_array_size(windows), &bounding_box);

            app_update_primary(&primary_record, &primary_atlas, &directory);
            app_update_primary_line_number(&primary_record, &primary_line_number);
            tui_element_scrollable_update(&scrollable_primary, 0, true, true);
            app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &directory);
            tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

            tui_element_scrollable_draw(&scrollable_primary_line_number);
            tui_element_scrollable_draw(&scrollable_primary);
            tui_element_scrollable_draw(&scrollable_secondary);
            tui_element_text_draw(&text_path, string_from_cstring(directory.data));
            tui_element_line_vertical_draw(&line, '#');
        }

        ssize_t count = read(STDIN_FILENO, &input, 1);
        if(count < 0) {
            return error_read;
        } else if(count > 0) {
            switch(input) {
                case 'q': break;
                case 'j':
                    tui_element_scrollable_update(&scrollable_primary_line_number, 1, true, true);
                    tui_element_scrollable_update(&scrollable_primary, 1, true, true);
                    app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &directory);
                    tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                    tui_element_scrollable_draw(&scrollable_primary_line_number);
                    tui_element_scrollable_draw(&scrollable_primary);
                    tui_element_scrollable_draw(&scrollable_secondary);
                    break;

                case 'k':
                    tui_element_scrollable_update(&scrollable_primary_line_number, 1, false, true);
                    tui_element_scrollable_update(&scrollable_primary, 1, false, true);
                    app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &directory);
                    tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                    tui_element_scrollable_draw(&scrollable_primary_line_number);
                    tui_element_scrollable_draw(&scrollable_primary);
                    tui_element_scrollable_draw(&scrollable_secondary);
                    break;

                case 'h':
                    app_path_up(&directory);
                    app_update_primary(&primary_record, &primary_atlas, &directory);
                    app_update_primary_line_number(&primary_record, &primary_line_number);
                    tui_element_scrollable_update(&scrollable_primary_line_number, 0, true, true);
                    tui_element_scrollable_update(&scrollable_primary, 0, true, true);
                    app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &directory);
                    tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                    tui_element_scrollable_draw(&scrollable_primary_line_number);
                    tui_element_scrollable_draw(&scrollable_primary);
                    tui_element_scrollable_draw(&scrollable_secondary);
                    tui_element_text_draw(&text_path, string_from_cstring(directory.data));
                    break;

                case 'l':
                    app_path_down(&directory, &primary_record, &scrollable_primary);
                    app_update_primary(&primary_record, &primary_atlas, &directory);
                    app_update_primary_line_number(&primary_record, &primary_line_number);
                    tui_element_scrollable_update(&scrollable_primary_line_number, 0, true, true);
                    tui_element_scrollable_update(&scrollable_primary, 0, true, true);
                    app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &directory);
                    tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                    tui_element_scrollable_draw(&scrollable_primary_line_number);
                    tui_element_scrollable_draw(&scrollable_primary);
                    tui_element_scrollable_draw(&scrollable_secondary);
                    tui_element_text_draw(&text_path, string_from_cstring(directory.data));
                    break;

                default: break;
            }
        }

        usleep(5000);
    } while (input != 'q');

    error = terminal_restore_settings(&settings_saved);
    if (error != error_none) {
        return error;
    }

    record_free(&primary_record);
    record_free(&secondary_record);
    atlas_free(&primary_line_number);
    atlas_free(&primary_atlas);
    atlas_free(&secondary_atlas);
    string_builder_free(&directory);

    return error_none;
}
