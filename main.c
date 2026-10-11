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

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include "auxiliary.h"

typedef enum Error {
    error_none = 0,
    error_path,
    error_epoll,
    error_epoll_ctl,
    error_opendir,
    error_malloc,
    error_readdir,
    error_stat,
    error_getcwd,
    error_setvbuf,
    error_tcgetattr,
    error_tcsetattr,
    error_poll,
    error_read,
    error_tui_check,
    error_tui_update,
    error_monitor_input,
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

int record_callback_sort_inode(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].inode, entries->data[data_right].inode);
}

int record_callback_sort_type(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entry_convert_type_to_character(&entries->data[data_left]), entry_convert_type_to_character(&entries->data[data_right]));
}

int record_callback_sort_permissions(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare((entries->data[data_left].mode & MODE_PERMISSIONS_MASK), (entries->data[data_right].mode & MODE_PERMISSIONS_MASK));
}

int record_callback_sort_link_count(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].link_count, entries->data[data_right].link_count);
}

int record_callback_sort_uid(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].uid, entries->data[data_right].uid);
}

int record_callback_sort_gid(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].gid, entries->data[data_right].gid);
}

int record_callback_sort_size(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].size, entries->data[data_right].size);
}

int record_callback_sort_time_modified(size_t data_left, size_t data_right, void* context) {
    Entries* entries = (Entries*)context;
    return general_compare(entries->data[data_left].time_modified, entries->data[data_right].time_modified);
}

int record_callback_sort_name(size_t data_left, size_t data_right, void* context) {
    Atlas* names = (Atlas*)context;
    return strcmp(atlas_get_cstring_at_index(names, data_left), atlas_get_cstring_at_index(names, data_right));
}

void record_create_sorted_map(Record *record, Sort sort) {
    dynamic_array_reset(&record->map);
    for(size_t i = 0; i < record->entries.count; i++) {
        dynamic_array_append(&record->map, i);
    }

    switch(sort) {
        case SORT_NONE: break;
        case SORT_INODE: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_inode, &record->entries); break;
        case SORT_TYPE: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_type, &record->entries); break;
        case SORT_PERMISSIONS: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_permissions, &record->entries); break;
        case SORT_LINK_COUNT: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_link_count, &record->entries); break;
        case SORT_UID: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_uid, &record->entries); break;
        case SORT_GID: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_gid, &record->entries); break;
        case SORT_SIZE: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_size, &record->entries); break;
        case SORT_TIME_MODIFIED: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_time_modified, &record->entries); break;
        case SORT_NAME: quicksort(record->map.data, 0, record->entries.count - 1, record_callback_sort_name, &record->names); break;
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

Error terminal_apply_settings(struct termios* settings_saved) {
    const char data[] =
        ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER
        ASCII_CURSOR_MOVE_TO_HOME
        ASCII_PRIVATE_CURSOR_INVISIBLE;
    const size_t data_length =
        ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER_LENGTH +
        ASCII_CURSOR_MOVE_TO_HOME_LENGTH +
        ASCII_PRIVATE_CURSOR_INVISIBLE_LENGTH;

    write(STDOUT_FILENO, data, data_length);

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

    const char data[] =
        ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER
        ASCII_RESET
        ASCII_PRIVATE_CURSOR_VISIBLE;
    const size_t data_length =
        ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER_LENGTH +
        ASCII_RESET_LENGTH +
        ASCII_PRIVATE_CURSOR_VISIBLE_LENGTH;

    write(STDOUT_FILENO, data, data_length);

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
    uint8_t inode_width;
    uint8_t link_count_width;
    uint8_t size_width;
    uint8_t uid_width;
    uint8_t gid_width;

    // todo: convert to fixed column size
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

Error app_update_primary(Record *record, Atlas *record_atlas, String_Builder *path) {
    record_reset(record);
    atlas_reset(record_atlas);

    Error error = record_read_directory(record, string_from_cstring(path->data));
    if (error != error_none) {
        return error;
    }

    record_create_sorted_map(record, SORT_NAME);
    record_convert_to_atlas(record, record_atlas);

    return error_none;
}

Error app_update_primary_line_number(Record *record, Atlas *line_number) {
    const size_t index_width = 4;

    atlas_reset(line_number);

    for(size_t i = 0; i < record->entries.count; i++) {
        String_Builder* string_builder = atlas_string_builder_begin(line_number);
        // string_builder_append_cstring(string_builder, ASCII_MODE_ENABLE_DIM);
        string_builder_append_integer(string_builder, i + 1, index_width, ' ');
        // string_builder_append_cstring(string_builder, ":" ASCII_MODE_DISABLE_DIM " ");
        string_builder_append_cstring(string_builder, ": ");
        atlas_string_builder_end(line_number);
    }

    return error_none;
}

Error app_update_secondary(Record *record, Tui_Element_Scrollable *scrollable, Record *record_preview, Atlas *record_preview_atlas, String_Builder *path) {
    const size_t index = record->map.data[scrollable->selection];
    Entry *const entry = &record->entries.data[index];
    const String entry_name = atlas_get_string_at_index(&record->names, index);

    if (entry_convert_type_to_character(entry) == 'd') {
        String_Builder path_local = {0};

        record_reset(record_preview);
        atlas_reset(record_preview_atlas);

        string_builder_append_string(&path_local, string_from_cstring(path->data));
        string_builder_append_character(&path_local, 0);
        path_traverse_down(&path_local, entry_name);

        Error error = record_read_directory(record_preview, string_from_cstring(path_local.data));
        if (error != error_none) {
            string_builder_free(&path_local);
            return error;
        }

        record_create_sorted_map(record_preview, SORT_NAME);
        record_convert_to_atlas(record_preview, record_preview_atlas);
        string_builder_free(&path_local);
    }

    return error_none;
}

Error app_path_up(String_Builder *path) {
    path_traverse_up(path);
    return error_none;
}

Error app_path_down(String_Builder *path, Record *record, Tui_Element_Scrollable *scrollable) {
    const size_t index = record->map.data[scrollable->selection];
    Entry *const entry = &record->entries.data[index];
    const String entry_name = atlas_get_string_at_index(&record->names, index);

    if (entry_convert_type_to_character(entry) == 'd') {
        path_traverse_down(path, entry_name);
    }

    return error_none;
}

Error app_setup_input(int *input_fd) {
    *input_fd = epoll_create1(0);
    if (*input_fd == -1) {
        return error_epoll;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;
    int error = epoll_ctl(*input_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event);
    if (error) {
        return error_epoll_ctl;
    }

    return error_none;
}

typedef enum Monitor_Input {
    monitor_input_skip,
    monitor_input_process,
    monitor_input_error,
} Monitor_Input;

Monitor_Input app_monitor_input(int input_fd, uint8_t* input) {
    struct epoll_event event;
    int event_count = epoll_wait(input_fd, &event, 1, -1);
    if(event_count < 0) {
        switch (errno) {
            case EINTR:
                return monitor_input_skip;
            default:
                return monitor_input_error;
        }
    } else if(event_count > 0) {
        if (event.events & (EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLRDHUP)) {
            return monitor_input_error;
        }

        if (!(event.events & EPOLLIN)) {
            return monitor_input_skip;
        }

        ssize_t count = read(STDIN_FILENO, input, 1);
        if(count < 0) {
            return monitor_input_error;
        } else if(count > 0) {
            return monitor_input_process;
        }
    }

    return monitor_input_skip;
}

int main(int argc, char** argv) {
    String_Builder path = {0};

    const char *directory_input;
    if (argc == 1) {
        directory_input = ".";
    } else {
        directory_input = argv[1];
    }

    if (path_normalize_from_string(&path, string_from_cstring((char*)directory_input))) {
        return error_path;
    }

    Error error;
    int input_fd;
    Record primary_record = {0};
    Record secondary_record = {0};
    Atlas primary_line_number = {0};
    Atlas primary_atlas = {0};
    Atlas secondary_atlas = {0};

    error = app_setup_input(&input_fd);
    if (error != error_none) {
        return error;
    }

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

    // todo: add format to elements
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

    enum Vim_Id {
        vim_id_quit,
        vim_id_left,
        vim_id_right,
        vim_id_up,
        vim_id_down,
        vim_id_top,
        vim_id_bottom,
    };

    const Vim_Motion motions_branch_g[] = {
        vim_motion_make_leaf('g', vim_id_top),
    };

    const Vim_Motion motions[] = {
        vim_motion_make_leaf('q', vim_id_quit),
        vim_motion_make_leaf('h', vim_id_left),
        vim_motion_make_leaf('j', vim_id_down),
        vim_motion_make_leaf('k', vim_id_up),
        vim_motion_make_leaf('l', vim_id_right),
        vim_motion_make_branch('g', motions_branch_g),
        vim_motion_make_leaf('G', vim_id_bottom),
    };

    uint8_t input = 0;
    Vim vim = {0};
    vim.motions = motions;
    vim.motions_count = general_array_size(motions);
    vim_reset(&vim);

    do {
        if (terminal_size_updated) {
            terminal_size_updated = 0;
            write(STDOUT_FILENO, ASCII_ERASE_SCREEN, ASCII_ERASE_SCREEN_LENGTH);

            Tui_Bounding_Box bounding_box = {.x = 1, .y = 1, .width = terminal_size.ws_col, .height = terminal_size.ws_row};
            tui_update(windows, windows_scratchpad, general_array_size(windows), &bounding_box);

            app_update_primary(&primary_record, &primary_atlas, &path);
            app_update_primary_line_number(&primary_record, &primary_line_number);
            tui_element_scrollable_update(&scrollable_primary, 0, true, true);
            app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
            tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

            tui_element_scrollable_draw(&scrollable_primary_line_number);
            tui_element_scrollable_draw(&scrollable_primary);
            tui_element_scrollable_draw(&scrollable_secondary);
            tui_element_text_draw(&text_path, string_from_cstring(path.data));
            tui_element_line_vertical_draw(&line, '#');
        }

        switch(app_monitor_input(input_fd, &input)) {
            case monitor_input_skip: continue;
            case monitor_input_process: break;
            case monitor_input_error: return error_monitor_input;
        }

        switch(vim_advance(&vim, input)) {
            case vim_advance_skip: continue;
            case vim_advance_done: break;
        }

        switch(vim.action.id) {
            case vim_id_quit: break;
            case vim_id_down:
                tui_element_scrollable_update(&scrollable_primary_line_number, vim.action.count, true, true);
                tui_element_scrollable_update(&scrollable_primary, vim.action.count, true, true);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                break;

            case vim_id_up:
                tui_element_scrollable_update(&scrollable_primary_line_number, vim.action.count, false, true);
                tui_element_scrollable_update(&scrollable_primary, vim.action.count, false, true);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                break;

            case vim_id_top:
                tui_element_scrollable_update(&scrollable_primary_line_number, vim.action.count - 1, true, false);
                tui_element_scrollable_update(&scrollable_primary, vim.action.count - 1, true, false);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                break;

            case vim_id_bottom:
                tui_element_scrollable_update(&scrollable_primary_line_number, SIZE_MAX, true, false);
                tui_element_scrollable_update(&scrollable_primary, SIZE_MAX, true, false);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                break;

            case vim_id_left:
                app_path_up(&path);
                app_update_primary(&primary_record, &primary_atlas, &path);
                app_update_primary_line_number(&primary_record, &primary_line_number);
                tui_element_scrollable_update(&scrollable_primary_line_number, 0, true, true);
                tui_element_scrollable_update(&scrollable_primary, 0, true, true);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                tui_element_text_draw(&text_path, string_from_cstring(path.data));
                break;

            case vim_id_right:
                app_path_down(&path, &primary_record, &scrollable_primary);
                app_update_primary(&primary_record, &primary_atlas, &path);
                app_update_primary_line_number(&primary_record, &primary_line_number);
                tui_element_scrollable_update(&scrollable_primary_line_number, 0, true, true);
                tui_element_scrollable_update(&scrollable_primary, 0, true, true);
                app_update_secondary(&primary_record, &scrollable_primary, &secondary_record, &secondary_atlas, &path);
                tui_element_scrollable_update(&scrollable_secondary, 0, true, true);

                tui_element_scrollable_draw(&scrollable_primary_line_number);
                tui_element_scrollable_draw(&scrollable_primary);
                tui_element_scrollable_draw(&scrollable_secondary);
                tui_element_text_draw(&text_path, string_from_cstring(path.data));
                break;

            default: break;
        }
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
    string_builder_free(&path);

    return error_none;
}
