#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
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
typedef struct String_Builder {
    char* data;
    size_t size;
    size_t count;
} String_Builder;

// void string_builder_reserve(String_Builder* string_builder, size_t count) {
//     while (string_builder->count + count > string_builder->size) {
//         string_builder->size = string_builder->size == 0 ? 512 : string_builder->size * 2;
//         string_builder->data = (char*)realloc(string_builder->data, string_builder->size * sizeof(*string_builder->data));
//         assert(string_builder->data != NULL);
//     }
// }

static void string_builder_reset(String_Builder* string_builder) {
    string_builder->count = 0;
}

static void string_builder_append_character(String_Builder* string_builder, char character) {
    dynamic_array_append(string_builder, character);
}

static void string_builder_append_string(String_Builder* string_builder, char* string) {
    size_t length = strlen(string);
    dynamic_array_reserve(string_builder, length);

    memcpy(string_builder->data + string_builder->count, string, length);
    string_builder->count += length;
}

static void string_builder_free(String_Builder* string_builder) {
    free(string_builder->data);
}

typedef struct Index {
    size_t index;
    size_t length;
} Index;

typedef struct Atlas {
    struct Indecies {
        Index* data;
        size_t size;
        size_t count;
    } indecies;
    struct Characters {
        char* data;
        size_t size;
        size_t count;
    } characters;
} Atlas;

static void atlas_append(Atlas* atlas, char* string) {
    size_t length = strlen(string) + 1;
    dynamic_array_reserve(&atlas->indecies, 1);
    dynamic_array_reserve(&atlas->characters, length);

    atlas->indecies.data[atlas->indecies.count] = (Index){.index = atlas->characters.count, .length = length};
    atlas->indecies.count++;
    memcpy(atlas->characters.data + atlas->characters.count, string, length);
    atlas->characters.count += length;
}

static char* atlas_get_string_at_index(Atlas* atlas, size_t index) {
    return &atlas->characters.data[atlas->indecies.data[index].index];
}

static void atlas_free(Atlas* atlas) {
    free(atlas->indecies.data);
    free(atlas->characters.data);
}

#define ASCII_ESCAPE "\x1B"

#define ASCII_CURSOR_MOVE_TO_HOME ASCII_ESCAPE"[H"
#define ASCII_CURSOR_MOVE_TO_POSITION ASCII_ESCAPE"[%d;%dH"
#define ASCII_CURSOR_MOVE_UP ASCII_ESCAPE"[%dA"
#define ASCII_CURSOR_MOVE_DOWN ASCII_ESCAPE"[%dB"
#define ASCII_CURSOR_MOVE_RIGHT ASCII_ESCAPE"[%dC"
#define ASCII_CURSOR_MOVE_LEFT ASCII_ESCAPE"[%dD"
#define ASCII_CURSOR_MOVE_TO_COLUMN ASCII_ESCAPE"[#G"
#define ASCII_CURSOR_SAVE_POSITION_DEC "ESC 7"
#define ASCII_CURSOR_RESTORE_POSITION_DEC "ESC 8"
#define ASCII_CURSOR_SAVE_POSITION_SCO "ESC[s"
#define ASCII_CURSOR_RESTORE_POSITION_SCO "ESC[u"

#define ASCII_ERASE_DISPLAY ASCII_ESCAPE"[J"
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_END ASCII_ESCAPE"[0J"
#define ASCII_ERASE_FROM_CURSOR_TO_SCREEN_BEGIN ASCII_ESCAPE"[1J"
#define ASCII_ERASE_SCREEN ASCII_ESCAPE"[2J"
#define ASCII_ERASE_SAVED_LINES ASCII_ESCAPE"[3J"
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_END ASCII_ESCAPE"[K"
#define ASCII_ERASE_FROM_CURSOR_TO_LINE_START ASCII_ESCAPE"[1K"
#define ASCII_ERASE_LINE ASCII_ESCAPE"[2K"

#define ASCII_RESET ASCII_ESCAPE"[0m"

#define ASCII_MODE_ENABLE_BOLD ASCII_ESCAPE"[1m"
#define ASCII_MODE_ENABLE_DIM ASCII_ESCAPE"[2m"
#define ASCII_MODE_ENABLE_ITALIC ASCII_ESCAPE"[3m"
#define ASCII_MODE_ENABLE_UNDERLINE ASCII_ESCAPE"[4m"
#define ASCII_MODE_ENABLE_BLINKING ASCII_ESCAPE"[5m"
#define ASCII_MODE_ENABLE_INVERSE ASCII_ESCAPE"[7m"
#define ASCII_MODE_ENABLE_HIDDEN ASCII_ESCAPE"[8m"
#define ASCII_MODE_ENABLE_STRIKETHROUGH ASCII_ESCAPE"[9m"
#define ASCII_MODE_DISABLE_BOLD ASCII_ESCAPE"[22m"
#define ASCII_MODE_DISABLE_DIM ASCII_ESCAPE"[22m"
#define ASCII_MODE_DISABLE_ITALIC ASCII_ESCAPE"[23m"
#define ASCII_MODE_DISABLE_UNDERLINE ASCII_ESCAPE"[24m"
#define ASCII_MODE_DISABLE_BLINKING ASCII_ESCAPE"[25m"
#define ASCII_MODE_DISABLE_INVERSE ASCII_ESCAPE"[27m"
#define ASCII_MODE_DISABLE_HIDDEN ASCII_ESCAPE"[28m"
#define ASCII_MODE_DISABLE_STRIKETHROUGH ASCII_ESCAPE"[29m"

#define ASCII_COLOR_FOREGROUND_BLACK ASCII_ESCAPE"[30m"
#define ASCII_COLOR_FOREGROUND_RED ASCII_ESCAPE"[31m"
#define ASCII_COLOR_FOREGROUND_GREEN ASCII_ESCAPE"[32m"
#define ASCII_COLOR_FOREGROUND_YELLOW ASCII_ESCAPE"[33m"
#define ASCII_COLOR_FOREGROUND_BLUE ASCII_ESCAPE"[34m"
#define ASCII_COLOR_FOREGROUND_MAGENTA ASCII_ESCAPE"[35m"
#define ASCII_COLOR_FOREGROUND_CYAN ASCII_ESCAPE"[36m"
#define ASCII_COLOR_FOREGROUND_WHITE ASCII_ESCAPE"[37m"
#define ASCII_COLOR_FOREGROUND_DEFAULT ASCII_ESCAPE"[39m"
#define ASCII_COLOR_BACKGROUND_BLACK ASCII_ESCAPE"[40m"
#define ASCII_COLOR_BACKGROUND_RED ASCII_ESCAPE"[41m"
#define ASCII_COLOR_BACKGROUND_GREEN ASCII_ESCAPE"[42m"
#define ASCII_COLOR_BACKGROUND_YELLOW ASCII_ESCAPE"[43m"
#define ASCII_COLOR_BACKGROUND_BLUE ASCII_ESCAPE"[44m"
#define ASCII_COLOR_BACKGROUND_MAGENTA ASCII_ESCAPE"[45m"
#define ASCII_COLOR_BACKGROUND_CYAN ASCII_ESCAPE"[46m"
#define ASCII_COLOR_BACKGROUND_WHITE ASCII_ESCAPE"[47m"
#define ASCII_COLOR_BACKGROUND_DEFAULT ASCII_ESCAPE"[49m"

#define ASCII_PRIVATE_CURSOR_INVISIBLE ASCII_ESCAPE"[?25l"
#define ASCII_PRIVATE_CURSOR_VISIBLE ASCII_ESCAPE"[?25h"
#define ASCII_PRIVATE_RESTORE_SCREEN ASCII_ESCAPE"[?47l"
#define ASCII_PRIVATE_STORE_SCREEN ASCII_ESCAPE"[?47h"
#define ASCII_PRIVATE_ENABLE_ALTERNATIVE_BUFFER ASCII_ESCAPE"[?1049h"
#define ASCII_PRIVATE_DISABLE_ALTERNATIVE_BUFFER ASCII_ESCAPE"[?1049l"

#define general_array_size(array) (sizeof((array)) / sizeof((array)[0]))

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

#define tui_window_make_text(id_parent, id_child, size_child) \
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

// static void tui_attach_layout_column(Tui_Window* parent_window, Tui_Size* widths, Tui_Window* children_window, uint8_t count) {
// }

// static Tui_Error tui_attach_layout_row(Tui_Window* parent_window, Tui_Size* heights, Tui_Window* children_window, uint8_t count) {
//     for(size_t i = 0; i < count; i++) {
//         uint32_t height;
//
//         switch(heights[i].kind) {
//             case TUI_SIZE_KIND_FIXED:
//                 height = heights[i].as.fixed;
//                 break;
//             case TUI_SIZE_KIND_RATIO:
//                 break;
//             case TUI_SIZE_KIND_FILL:
//                 break;
//             default:
//                 break;
//         }
//
//         children_window->bounding_box = (Tui_Bounding_Box){
//             .x = parent_window->bounding_box.x,
//             .y = parent_window->bounding_box.y,
//             .width = parent_window->bounding_box.width,
//             .height = parent_window->bounding_box.height,
//         };
//         dynamic_array_append(&parent_window->children, children_window[i]);
//     }
//
//     return tui_error_none;
// }

#endif

// root(100, 40)
//    layout_row(100, 40, (1, *, 1), 3)
//        [0] =>
//            text(100, 1)
//        [1] =>
//            layout_column(100, 38, (0.3p, 1, *), 3)
//                [0] =>
//                    text(33, 38)
//                [1] =>
//                    vline(1, 38)
//                [2] =>
//                    text(66, 38)
//        [2] =>
//            text(100, 1)
