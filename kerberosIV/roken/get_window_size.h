#ifndef GET_WINDOW_SIZE_H
#define GET_WINDOW_SIZE_H
struct winsize {
    unsigned short ws_row, ws_col;
    unsigned short ws_xpixel, ws_ypixel;
};

int get_window_size(int fd, struct winsize *ws);
#endif
