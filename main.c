/***************\
|*** include ***|
\***************/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/**************\
|*** define ***|
\**************/
#define tedVER "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

/************\
|*** data ***|
\************/
struct editorConfig {               // global struct containing our editor state
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;    // store global clean copy of terminal attributes
};

struct editorConfig E;              // initialize as variable E

/****************\
|*** terminal ***|
\****************/
void die(const char* s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);  // using perror for descriptiveness and ability to print the string before throwing error
    exit(1);
}

void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattrD");
}

void enableRawMode(void)
{ 
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");     /* read terminal attributes into orig_termios */ 
    atexit(disableRawMode);                                                 /* register disableRawMode() to be called automatically when program exits*/ 
    struct termios raw = E.orig_termios;                                      /* declare variable raw of type struct termios and populate it with the clean copy */ 
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);               /* Disable ctrl s/q with IXON input flag. Fix ctrl m from reading as 10 with ICRNL. And other stuff. */
    raw.c_oflag &= ~(OPOST);                                                /* Turn off output translation, NO MORE \r\n unless I SAY SO!!! */
    raw.c_cflag |= (CS8);                                                   /* just in case */

                                                                            // Perform bitwise-NOT on echo (bitflag), then bitwise-AND with the flags field
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);                        // to properly bit flip ECHO, then OR with ICANON to disable canonical mode. 
                                                                            // Use ISIG flag to disable ctrl c/z signals. Use IEXTEN flag to disable ctrl v. 

    raw.c_cc[VMIN] = 0;                                                     // read() requires 0 bytes to return, so it returns upon input
    raw.c_cc[VTIME] = 1;                                                    // read() waits 1/10 of a second to return

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattrE");  // set new attributes from raw
}

char editorReadKey(void)
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int* rows, int* cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int* rows, int* cols)
{
    // structure holding window dimensions
    struct winsize ws;
    // call ioctl to get window size, return if ioctl fails or columns returns as 0
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;    // C (cursor forward), B (cursor down)
        return getCursorPosition(rows, cols);
    // otherwise, dereference function params and assign them with the updated rows and columns
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*********************\
|*** append buffer ***|
\*********************/
struct abuf {
    char* b;                    // pointer to the buffer in mem
    int len;                    // its length
};

#define ABUF_INIT {NULL, 0}     // constant representing an empty buffer, also acts as a constructor for the abuf type

void abAppend(struct abuf* ab, const char* s, int len)
{
    char* new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf* ab)
{
    free(ab->b);
}

/**************\
|*** output ***|
\**************/
void editorDrawRows(struct abuf* ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++) {
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "teditor -- version %s", tedVER);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen(void)
{
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
                                        // writing out 4 bytes to the terminal. First, "\x1b" is the escape character, 27 in decimal
                                        // 2nd, 3rd, and 4th are [, 2, and J, respectively. escape sequences always start with the
                                        // escape char followed by a [. The command used here is J (Erase In Display), with 2 being
//    write(STDOUT_FILENO, "\x1b[2J", 4); // its argument which says to clear the whole screen. we are using VT100 esc sequences.
    
//    write(STDOUT_FILENO, "\x1b[H", 3);  // H command (cursor pos) will put cursor at the first row & first column 

//    editorDrawRows();

//    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*************\
|*** input ***|
\*************/
void editorProcessKeypress(void)
{
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/************\
|*** init ***|
\************/
void initEditor(void)
{
    E.cx = 0;
    E.cy = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(void) {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
                    // char c = '\0';
                    // if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
                    // if (iscntrl(c)) {           /* from ctype.h--tests whether character is a control (0-31, 127) */
                    //
                    //     /* print integer representation of control key */
                    //     printf("%d\r\n", c);
                    //
                    // } else {
                    //
                    //     /* otherwise, also print the character */
                    //     printf("%d ('%c')\r\n", c, c);
                    //
                    // }
                    //
                    // if (c == CTRL_KEY('q')) break;
    return 0;
}
