#ifndef __BOX_LIB__H_____
#define __BOX_LIB__H_____

typedef struct {
    int width;
    int left;
    int top;
    int color;
    int shift_color;
    int focus_border_color;
    int blur_border_color;
    int text_color;
    int max;
    char *text;
    int mouse_variable;
    int flags;
    int size;
    int pos;
    int offset;
    int cl_curs_x;
    int cl_curs_y;
    int shift;
    int shift_old;
} EditBox;

#endif // __BOX_LIB__H_____