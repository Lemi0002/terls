#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

#define general_array_size(array) (sizeof((array)) / sizeof((array)[0]))
#define general_min(a, b) ((a) > (b) ? (b) : (a))
#define general_max(a, b) ((a) > (b) ? (a) : (b))
#define general_absolute(a) ((a) < 0 ? -(a) : (a))
#define general_sign(a) ((a) < 0 ? -1 : 1)

#define dynamic_array_fields(data_type) \
    (data_type)* data; \
    size_t size; \
    size_t count

#define dynamic_array_free(dynamic_array) free((dynamic_array)->data)

#define dynamic_array_reserve(dynamic_array, reserve) \
    do { \
            while ((dynamic_array)->count + (reserve) > (dynamic_array)->size) { \
                (dynamic_array)->size = (dynamic_array)->size == 0 ? 512 : (dynamic_array)->size * 2; \
                (dynamic_array)->data = (typeof((dynamic_array)->data))realloc((dynamic_array)->data, (dynamic_array)->size * sizeof(*(dynamic_array)->data)); \
                assert((dynamic_array)->data != NULL); \
            } \
    } while(0)

#define dynamic_array_append(dynamic_array, value) \
    do { \
        dynamic_array_reserve((dynamic_array), 1); \
        (dynamic_array)->data[(dynamic_array)->count] = (value); \
        (dynamic_array)->count++; \
    } while (0)

typedef struct String {
    char* cstring;
    size_t length;
} String;

static String string_from_cstring(char* cstring) {
    return (String){
        .cstring = cstring,
        .length = strlen(cstring)
    };
}

typedef struct String_Builder {
    char* data;
    size_t size;
    size_t count;
    size_t control_character_count;
} String_Builder;

static void string_builder_reserve(String_Builder* string_builder, size_t count) {
    dynamic_array_reserve(string_builder, count);
}

static void string_builder_reset(String_Builder* string_builder) {
    string_builder->count = 0;
    string_builder->control_character_count = 0;
}

static void string_builder_append_character(String_Builder* string_builder, char character) {
    dynamic_array_append(string_builder, character);
}

static void string_builder_append_cstring(String_Builder* string_builder, char* cstring) {
    size_t length = strlen(cstring);
    dynamic_array_reserve(string_builder, length);

    memcpy(string_builder->data + string_builder->count, cstring, length);
    string_builder->count += length;
}

static void string_builder_append_string(String_Builder* string_builder, String string) {
    dynamic_array_reserve(string_builder, string.length);

    memcpy(string_builder->data + string_builder->count, string.cstring, string.length);
    string_builder->count += string.length;
}

static void string_builder_append_integer(String_Builder* string_builder, size_t data, const uint8_t width, const char fill_character) {
    bool fill = false;
    dynamic_array_reserve(string_builder, width);

    for(uint8_t i = width; i > 0; --i) {
        if(fill) {
            string_builder->data[string_builder->count + i - 1] = fill_character;
        } else {
            string_builder->data[string_builder->count + i - 1] = '0' + (data % 10);
        }

        data = data / 10;

        if(data == 0) {
            fill = true;
        }
    }
    string_builder->count += width;
}

static void string_builder_increment_control_character_count(String_Builder* string_builder, size_t control_character_count) {
    string_builder->control_character_count += control_character_count;
}

static void string_builder_free(String_Builder* string_builder) {
    free(string_builder->data);
}

typedef struct Index {
    size_t head;
    size_t length;
    size_t length_visible;
} Index;

typedef struct Atlas {
    struct Indecies {
        Index* data;
        size_t size;
        size_t count;
    } indecies;
    String_Builder string_builder;
    String_Builder string_builder_tempo;
} Atlas;

static void atlas_append_cstring(Atlas* atlas, char* cstring) {
    const size_t length = strlen(cstring);
    dynamic_array_append(&atlas->indecies, ((Index){.head = atlas->string_builder.count, .length = length, .length_visible = length}));
    string_builder_append_string(&atlas->string_builder, (String){.cstring = cstring, .length = length});
    string_builder_append_character(&atlas->string_builder, 0);
}

static void atlas_append_string(Atlas* atlas, String string) {
    const size_t length = string.length;
    dynamic_array_append(&atlas->indecies, ((Index){.head = atlas->string_builder.count, .length = length, .length_visible = length}));
    string_builder_append_string(&atlas->string_builder, (String){.cstring = string.cstring, .length = length});
    string_builder_append_character(&atlas->string_builder, 0);
}

static char* atlas_get_cstring_at_index(Atlas* atlas, size_t index) {
    return &atlas->string_builder.data[atlas->indecies.data[index].head];
}

static String atlas_get_string_at_index(Atlas* atlas, size_t index) {
    return (String) {
        .cstring = &atlas->string_builder.data[atlas->indecies.data[index].head],
        .length = atlas->indecies.data[index].length
    };
}

static void atlas_free(Atlas* atlas) {
    free(atlas->indecies.data);
    free(atlas->string_builder.data);
}

static String_Builder* atlas_string_builder_begin(Atlas* atlas) {
    atlas->string_builder_tempo = atlas->string_builder;
    return &atlas->string_builder;
}

static void atlas_string_builder_end(Atlas* atlas) {
    const size_t length = atlas->string_builder.count - atlas->string_builder_tempo.count;
    const size_t control_character_count = atlas->string_builder.control_character_count - atlas->string_builder_tempo.control_character_count;
    const size_t length_visible = length - general_min(length, control_character_count);
    dynamic_array_append(&atlas->indecies, ((Index){.head = atlas->string_builder_tempo.count, .length = length, .length_visible = length_visible}));
    string_builder_append_character(&atlas->string_builder, 0);
}

static inline size_t buffer_append_cstring(char* const buffer, const size_t offset, char* cstring) {
    const size_t length = strlen(cstring);
    memcpy(buffer + offset, cstring, length);
    return length;
}

static inline size_t buffer_append_character(char* const buffer, const size_t offset, const char character) {
    buffer[offset] = character;
    return 1;
}

static size_t buffer_append_integer(char* const buffer, const size_t offset, size_t data, const uint8_t width, const char fill_character) {
    bool fill = false;
    for(uint8_t i = width; i > 0; --i) {
        if(fill) {
            buffer[offset + i - 1] = fill_character;
        } else {
            buffer[offset + i - 1] = '0' + (data % 10);
        }

        data = data / 10;

        if(data == 0) {
            fill = true;
        }
    }
    return width;
}

#define ASCII_ESCAPE "\x1B"

#define ASCII_CURSOR_MOVE_TO_HOME ASCII_ESCAPE"[H"
#define ASCII_CURSOR_MOVE_TO_HOME_LENGTH 3

#define ASCII_CURSOR_MOVE_TO_POSITION ASCII_ESCAPE"[%d;%dH"
#define ASCII_CURSOR_MOVE_UP ASCII_ESCAPE"[%dA"
#define ASCII_CURSOR_MOVE_DOWN ASCII_ESCAPE"[%dB"
#define ASCII_CURSOR_MOVE_RIGHT ASCII_ESCAPE"[%dC"
#define ASCII_CURSOR_MOVE_LEFT ASCII_ESCAPE"[%dD"
#define ASCII_CURSOR_MOVE_TO_COLUMN ASCII_ESCAPE"[%dG"

#define ASCII_CURSOR_SAVE_POSITION_DEC ASCII_ESCAPE"7"
#define ASCII_CURSOR_SAVE_POSITION_DEC_LENGTH 2
#define ASCII_CURSOR_RESTORE_POSITION_DEC ASCII_ESCAPE"8"
#define ASCII_CURSOR_RESTORE_POSITION_DEC_LENGTH 2
#define ASCII_CURSOR_SAVE_POSITION_SCO ASCII_ESCAPE"[s"
#define ASCII_CURSOR_SAVE_POSITION_SCO_LENGTH 3
#define ASCII_CURSOR_RESTORE_POSITION_SCO ASCII_ESCAPE"[u"
#define ASCII_CURSOR_RESTORE_POSITION_SCO_LENGTH 3

#define ASCII_ERASE_DISPLAY ASCII_ESCAPE"[J"
#define ASCII_ERASE_DISPLAY_LENGTH 3
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_END ASCII_ESCAPE"[0J"
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_END_LENGTH 4
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_BEGIN ASCII_ESCAPE"[1J"
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_BEGIN_LENGTH 4
#define ASCII_ERASE_SCREEN ASCII_ESCAPE"[2J"
#define ASCII_ERASE_SCREEN_LENGTH 4
#define ASCII_ERASE_SAVED_LINES ASCII_ESCAPE"[3J"
#define ASCII_ERASE_SAVED_LINES_LENGTH 4
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_END ASCII_ESCAPE"[K"
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_END_LENGTH 4
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_START ASCII_ESCAPE"[1K"
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_START_LENGTH 4
#define ASCII_ERASE_LINE ASCII_ESCAPE"[2K"
#define ASCII_ERASE_LINE_LENGTH 4

#define ASCII_RESET ASCII_ESCAPE"[0m"
#define ASCII_RESET_LENGTH 4

#define ASCII_MODE_ENABLE_BOLD ASCII_ESCAPE"[1m"
#define ASCII_MODE_ENABLE_BOLD_LENGTH 4
#define ASCII_MODE_ENABLE_DIM ASCII_ESCAPE"[2m"
#define ASCII_MODE_ENABLE_DIM_LENGTH 4
#define ASCII_MODE_ENABLE_ITALIC ASCII_ESCAPE"[3m"
#define ASCII_MODE_ENABLE_ITALIC_LENGTH 4
#define ASCII_MODE_ENABLE_UNDERLINE ASCII_ESCAPE"[4m"
#define ASCII_MODE_ENABLE_UNDERLINE_LENGTH 4
#define ASCII_MODE_ENABLE_BLINKING ASCII_ESCAPE"[5m"
#define ASCII_MODE_ENABLE_BLINKING_LENGTH 4
#define ASCII_MODE_ENABLE_INVERSE ASCII_ESCAPE"[7m"
#define ASCII_MODE_ENABLE_INVERSE_LENGTH 4
#define ASCII_MODE_ENABLE_HIDDEN ASCII_ESCAPE"[8m"
#define ASCII_MODE_ENABLE_HIDDEN_LENGTH 4
#define ASCII_MODE_ENABLE_STRIKETHROUGH ASCII_ESCAPE"[9m"
#define ASCII_MODE_ENABLE_STRIKETHROUGH_LENGTH 4
#define ASCII_MODE_DISABLE_BOLD ASCII_ESCAPE"[22m"
#define ASCII_MODE_DISABLE_BOLD_LENGTH 5
#define ASCII_MODE_DISABLE_DIM ASCII_ESCAPE"[22m"
#define ASCII_MODE_DISABLE_DIM_LENGTH 5
#define ASCII_MODE_DISABLE_ITALIC ASCII_ESCAPE"[23m"
#define ASCII_MODE_DISABLE_ITALIC_LENGTH 5
#define ASCII_MODE_DISABLE_UNDERLINE ASCII_ESCAPE"[24m"
#define ASCII_MODE_DISABLE_UNDERLINE_LENGTH 5
#define ASCII_MODE_DISABLE_BLINKING ASCII_ESCAPE"[25m"
#define ASCII_MODE_DISABLE_BLINKING_LENGTH 5
#define ASCII_MODE_DISABLE_INVERSE ASCII_ESCAPE"[27m"
#define ASCII_MODE_DISABLE_INVERSE_LENGTH 5
#define ASCII_MODE_DISABLE_HIDDEN ASCII_ESCAPE"[28m"
#define ASCII_MODE_DISABLE_HIDDEN_LENGTH 5
#define ASCII_MODE_DISABLE_STRIKETHROUGH ASCII_ESCAPE"[29m"
#define ASCII_MODE_DISABLE_STRIKETHROUGH_LENGTH 5

#define ASCII_COLOR_FOREGROUND_BLACK ASCII_ESCAPE"[30m"
#define ASCII_COLOR_FOREGROUND_BLACK_LENGTH 5
#define ASCII_COLOR_FOREGROUND_RED ASCII_ESCAPE"[31m"
#define ASCII_COLOR_FOREGROUND_RED_LENGTH 5
#define ASCII_COLOR_FOREGROUND_GREEN ASCII_ESCAPE"[32m"
#define ASCII_COLOR_FOREGROUND_GREEN_LENGTH 5
#define ASCII_COLOR_FOREGROUND_YELLOW ASCII_ESCAPE"[33m"
#define ASCII_COLOR_FOREGROUND_YELLOW_LENGTH 5
#define ASCII_COLOR_FOREGROUND_BLUE ASCII_ESCAPE"[34m"
#define ASCII_COLOR_FOREGROUND_BLUE_LENGTH 5
#define ASCII_COLOR_FOREGROUND_MAGENTA ASCII_ESCAPE"[35m"
#define ASCII_COLOR_FOREGROUND_MAGENTA_LENGTH 5
#define ASCII_COLOR_FOREGROUND_CYAN ASCII_ESCAPE"[36m"
#define ASCII_COLOR_FOREGROUND_CYAN_LENGTH 5
#define ASCII_COLOR_FOREGROUND_WHITE ASCII_ESCAPE"[37m"
#define ASCII_COLOR_FOREGROUND_WHITE_LENGTH 5
#define ASCII_COLOR_FOREGROUND_DEFAULT ASCII_ESCAPE"[39m"
#define ASCII_COLOR_FOREGROUND_DEFAULT_LENGTH 5
#define ASCII_COLOR_BACKGROUND_BLACK ASCII_ESCAPE"[40m"
#define ASCII_COLOR_BACKGROUND_BLACK_LENGTH 5
#define ASCII_COLOR_BACKGROUND_RED ASCII_ESCAPE"[41m"
#define ASCII_COLOR_BACKGROUND_RED_LENGTH 5
#define ASCII_COLOR_BACKGROUND_GREEN ASCII_ESCAPE"[42m"
#define ASCII_COLOR_BACKGROUND_GREEN_LENGTH 5
#define ASCII_COLOR_BACKGROUND_YELLOW ASCII_ESCAPE"[43m"
#define ASCII_COLOR_BACKGROUND_YELLOW_LENGTH 5
#define ASCII_COLOR_BACKGROUND_BLUE ASCII_ESCAPE"[44m"
#define ASCII_COLOR_BACKGROUND_BLUE_LENGTH 5
#define ASCII_COLOR_BACKGROUND_MAGENTA ASCII_ESCAPE"[45m"
#define ASCII_COLOR_BACKGROUND_MAGENTA_LENGTH 5
#define ASCII_COLOR_BACKGROUND_CYAN ASCII_ESCAPE"[46m"
#define ASCII_COLOR_BACKGROUND_CYAN_LENGTH 5
#define ASCII_COLOR_BACKGROUND_WHITE ASCII_ESCAPE"[47m"
#define ASCII_COLOR_BACKGROUND_WHITE_LENGTH 5
#define ASCII_COLOR_BACKGROUND_DEFAULT ASCII_ESCAPE"[49m"
#define ASCII_COLOR_BACKGROUND_DEFAULT_LENGTH 5

#define ASCII_PRIVATE_CURSOR_INVISIBLE ASCII_ESCAPE"[?25l"
#define ASCII_PRIVATE_CURSOR_INVISIBLE_LENGTH 6
#define ASCII_PRIVATE_CURSOR_VISIBLE ASCII_ESCAPE"[?25h"
#define ASCII_PRIVATE_CURSOR_VISIBLE_LENGTH 6
#define ASCII_PRIVATE_RESTORE_SCREEN ASCII_ESCAPE"[?47l"
#define ASCII_PRIVATE_RESTORE_SCREEN_LENGTH 6
#define ASCII_PRIVATE_STORE_SCREEN ASCII_ESCAPE"[?47h"
#define ASCII_PRIVATE_STORE_SCREEN_LENGTH 6
#define ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER ASCII_ESCAPE"[?1049h"
#define ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER_LENGTH 8
#define ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER ASCII_ESCAPE"[?1049l"
#define ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER_LENGTH 8

typedef enum Tui_Size_Kind {
    TUI_SIZE_KIND_FIXED,
    TUI_SIZE_KIND_RATIO,
    TUI_SIZE_KIND_FILL,
} Tui_Size_Kind;

typedef struct Tui_Size {
    enum Tui_Size_Kind kind;
    union {
        uint32_t fixed;
        float ratio;
    } as;
} Tui_Size;

#define tui_size_make_fixed(value) ((Tui_Size){.kind = TUI_SIZE_KIND_FIXED, .as.fixed = (value)})
#define tui_size_make_ratio(value) ((Tui_Size){.kind = TUI_SIZE_KIND_RATIO, .as.ratio = (value)})
#define tui_size_make_fill() ((Tui_Size){.kind = TUI_SIZE_KIND_FILL, .as.fixed = 0})

typedef struct Tui_Bounding_Box {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} Tui_Bounding_Box;

typedef enum Tui_Layout {
    TUI_LAYOUT_HORIZONTAL = 0,
    TUI_LAYOUT_VERTICAL = 1,
} Tui_Layout;

#define tui_bounding_box_position(bounding_box, layout) ((uint32_t*)(&(bounding_box)->x))[(layout)]
#define tui_bounding_box_length(bounding_box, layout) ((uint32_t*)(&(bounding_box)->width))[(layout)]

typedef enum Tui_Window_Kind {
    TUI_WINDOW_KIND_ROOT,
    TUI_WINDOW_KIND_LAYOUT,
    TUI_WINDOW_KIND_TEXT,
} Tui_Window_Kind;

typedef struct Tui_Window Tui_Window;
struct Tui_Window {
    uint32_t id;
    uint32_t parent;
    Tui_Window_Kind kind;
    Tui_Size size;
    Tui_Bounding_Box bounding_box;
    Tui_Layout layout;
    uint32_t child_count;
    // struct {
    //     Tui_Window** data;
    //     size_t size;
    //     size_t count;
    // } children;
};

#define out

typedef enum Tui_Error {
    TUI_ERROR_NONE,
    TUI_ERROR_PARAMETERS,
    TUI_ERROR_NO_ROOT_WINDOW,
    TUI_ERROR_MULTIPLE_ROOT_WINDOWS,
    TUI_ERROR_ROOT_WINDOW_INDEX,
    TUI_ERROR_MULTIPLE_FILL_SIZE_WINDOWS,
    TUI_ERROR_BOUNDING_BOX_TOO_SMALL,
} Tui_Error;

#define tui_window_number_of_arguments(...)  (sizeof((Tui_Window[]){__VA_ARGS__})/sizeof(Tui_Window))

#define tui_window_make_root(id_root, layout_root, ...) \
    (Tui_Window){ \
        .id = (id_root), \
        .parent = (id_root), \
        .kind = TUI_WINDOW_KIND_ROOT, \
        .size = {0}, \
        .bounding_box = {0}, \
        .layout = (layout_root), \
        .child_count = tui_window_number_of_arguments(__VA_ARGS__), \
    }, __VA_ARGS__

#define tui_window_make_layout(id_parent, id_child, size_child, layout_child, ...) \
    (Tui_Window){ \
        .id = (id_child), \
        .parent = (id_parent), \
        .kind = TUI_WINDOW_KIND_LAYOUT, \
        .size = (size_child), \
        .bounding_box = {0}, \
        .layout = (layout_child), \
        .child_count = tui_window_number_of_arguments(__VA_ARGS__), \
    }, __VA_ARGS__

#define tui_window_make_element(id_parent, id_child, size_child) \
    (Tui_Window){ \
        .id = (id_child), \
        .parent = (id_parent), \
        .kind = TUI_WINDOW_KIND_TEXT, \
        .size = (size_child), \
        .bounding_box = {0}, \
        .layout = 0, \
        .child_count = 0, \
    }

static void tui_child_windows(Tui_Window* windows, size_t count, size_t parent_index, Tui_Window** child_windows, size_t* child_count) {
    *child_count = 0;

    for(size_t i = parent_index + 1; i < count; i++) {
        if (windows[parent_index].id != windows[i].parent) {
            continue;
        }

        child_windows[*child_count] = &windows[i];
        (*child_count)++;
    }
}

static Tui_Error tui_check(Tui_Window* windows, Tui_Window** scratchpad, size_t count) {
    size_t root_index = 0;
    bool root_found = false;
    size_t fill_count = 0;

    for(size_t i = 0; i < count; i++) {
        switch(windows[i].kind) {
            case TUI_WINDOW_KIND_ROOT:
                if (root_found) {
                    return TUI_ERROR_MULTIPLE_ROOT_WINDOWS;
                }
                root_found = true;
                root_index = i;
                break;
            default:
                break;
        }
    }

    if (!root_found) {
        return TUI_ERROR_NO_ROOT_WINDOW;
    }

    if (root_index != 0) {
        return TUI_ERROR_ROOT_WINDOW_INDEX;
    }

    for(size_t i = 0; i < count; i++) {
        switch(windows[i].kind) {
            case TUI_WINDOW_KIND_ROOT:
            case TUI_WINDOW_KIND_LAYOUT: {
                size_t child_count;
                Tui_Window** child_windows = scratchpad;
                tui_child_windows(windows, count, i, child_windows, &child_count);

                size_t fill_count = 0;
                for(size_t j = 0; j < child_count; j++) {
                    if(child_windows[j]->size.kind == TUI_SIZE_KIND_FILL) {
                        fill_count++;
                    }
                }

                if(fill_count > 1) {
                    return TUI_ERROR_MULTIPLE_FILL_SIZE_WINDOWS;
                }
            } break;
            case TUI_WINDOW_KIND_TEXT:
                break;
            default:
                break;
        }
    }

    return TUI_ERROR_NONE;
}

static Tui_Error tui_update(Tui_Window* windows, Tui_Window** scratchpad, size_t count, Tui_Bounding_Box* bounding_box_root) {
    for(size_t i = 0; i < count; i++) {
        switch(windows[i].kind) {
            case TUI_WINDOW_KIND_ROOT:
                windows[i].bounding_box = *bounding_box_root;
            case TUI_WINDOW_KIND_LAYOUT: {
                size_t child_count;
                Tui_Window** child_windows = scratchpad;
                tui_child_windows(windows, count, i, child_windows, &child_count);

                Tui_Layout axis_primary;
                Tui_Layout axis_secondary;
                switch(windows[i].layout) {
                    case TUI_LAYOUT_VERTICAL:
                        axis_primary = TUI_LAYOUT_VERTICAL;
                        axis_secondary = TUI_LAYOUT_HORIZONTAL;
                        break;
                    case TUI_LAYOUT_HORIZONTAL:
                        axis_primary = TUI_LAYOUT_HORIZONTAL;
                        axis_secondary = TUI_LAYOUT_VERTICAL;
                        break;
                }

                Tui_Window* child_window_fill = NULL;
                uint32_t length = 0;
                uint32_t const parent_length = tui_bounding_box_length(&windows[i].bounding_box, axis_primary);
                for(size_t j = 0; j < child_count; j++) {
                    switch(child_windows[j]->size.kind) {
                        case TUI_SIZE_KIND_FIXED: {
                            uint32_t const child_length = child_windows[j]->size.as.fixed;
                            length += child_length;
                            tui_bounding_box_length(&child_windows[j]->bounding_box, axis_primary) = child_length;
                        } break;
                        case TUI_SIZE_KIND_RATIO: {
                            uint32_t const child_length = (uint32_t)(child_windows[j]->size.as.ratio * parent_length);
                            length += child_length;
                            tui_bounding_box_length(&child_windows[j]->bounding_box, axis_primary) = child_length;
                        } break;
                        case TUI_SIZE_KIND_FILL: {
                            child_window_fill = child_windows[j];
                        } break;
                    }
                    tui_bounding_box_position(&child_windows[j]->bounding_box, axis_secondary) = tui_bounding_box_position(&windows[i].bounding_box, axis_secondary);
                    tui_bounding_box_length(&child_windows[j]->bounding_box, axis_secondary) = tui_bounding_box_length(&windows[i].bounding_box, axis_secondary);
                }

                if(length > parent_length) {
                    return TUI_ERROR_BOUNDING_BOX_TOO_SMALL;
                }

                if(child_window_fill != NULL) {
                    tui_bounding_box_length(&child_window_fill->bounding_box, axis_primary) = parent_length - length;
                }

                uint32_t position = tui_bounding_box_position(&windows[i].bounding_box, axis_primary);
                for(size_t j = 0; j < child_count; j++) {
                    tui_bounding_box_position(&child_windows[j]->bounding_box, axis_primary) = position;
                    position += tui_bounding_box_length(&child_windows[j]->bounding_box, axis_primary);
                }
            } break;

            case TUI_WINDOW_KIND_TEXT:
                break;
            default:
                break;
        }
    }

    return TUI_ERROR_NONE;
}

typedef struct Tui_Element_Scrollable {
    Tui_Window* window;
    size_t offset;
    size_t selection;
    Atlas* atlas;
} Tui_Element_Scrollable;

static void tui_element_scrollable_update_selection(Tui_Element_Scrollable* scrollable, size_t value, bool positive, bool relative) {
    if (relative) {
        if (positive) {
            if (SIZE_MAX - scrollable->selection >= value) {
                scrollable->selection += value;
            } else {
                scrollable->selection = SIZE_MAX;
            }
        } else {
            if (scrollable->selection >= value) {
                scrollable->selection -= value;
            } else {
                scrollable->selection = 0;
            }
        }
    } else {
        scrollable->selection = value;
    }

    if(scrollable->selection >= scrollable->atlas->indecies.count) {
        if(scrollable->atlas->indecies.count == 0) {
            scrollable->selection = 0;
            return;
        } else {
            scrollable->selection = scrollable->atlas->indecies.count - 1;
        }
    }

    if(scrollable->selection < scrollable->offset) {
        scrollable->offset = scrollable->selection;
    } else if(scrollable->selection >= (scrollable->offset + scrollable->window->bounding_box.height)) {
        scrollable->offset = scrollable->selection - scrollable->window->bounding_box.height + 1;
    }

    printf(ASCII_CURSOR_MOVE_TO_POSITION ASCII_CURSOR_SAVE_POSITION_DEC, scrollable->window->bounding_box.y, scrollable->window->bounding_box.x);

    const char* postfix = ASCII_CURSOR_RESTORE_POSITION_DEC ASCII_ESCAPE"[1B" ASCII_CURSOR_SAVE_POSITION_DEC;
    const size_t postfix_length = strlen(postfix);
    const char* selection_prefix = ASCII_COLOR_BACKGROUND_BLUE;
    const size_t selection_prefix_length = strlen(selection_prefix);
    const char* selection_postifx = ASCII_RESET;
    const size_t selection_postfix_length = strlen(selection_postifx);
    const size_t index_max = general_min(scrollable->atlas->indecies.count, scrollable->offset + scrollable->window->bounding_box.height);

    for(size_t index = scrollable->offset; index < index_max; index++) {
        const String string = atlas_get_string_at_index(scrollable->atlas, index);
        //const size_t length_max = general_min(scrollable->atlas->indecies.data[index].length - 1, scrollable->window->bounding_box.width);

        if (scrollable->atlas->indecies.data[index].length_visible > scrollable->window->bounding_box.width) {
            continue;
        }

        const size_t length_max = scrollable->atlas->indecies.data[index].length;

        if (index == scrollable->selection) {
            struct iovec write_vector[4];
            write_vector[0].iov_base = (void*)selection_prefix;
            write_vector[0].iov_len = selection_prefix_length;
            write_vector[1].iov_base = (void*)string.cstring;
            write_vector[1].iov_len = length_max;
            write_vector[2].iov_base = (void*)selection_postifx;
            write_vector[2].iov_len = selection_postfix_length;
            write_vector[3].iov_base = (void*)postfix;
            write_vector[3].iov_len = postfix_length;
            writev(STDOUT_FILENO, write_vector, general_array_size(write_vector));
        } else {
            struct iovec write_vector[2];
            write_vector[0].iov_base = (void*)string.cstring;
            write_vector[0].iov_len = length_max;
            write_vector[1].iov_base = (void*)postfix;
            write_vector[1].iov_len = postfix_length;
            writev(STDOUT_FILENO, write_vector, general_array_size(write_vector));
        }
    }
}

#endif
