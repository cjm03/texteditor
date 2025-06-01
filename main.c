/***************\
|*** include ***|
\***************/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/************\
|*** data ***|
\************/
struct termios orig_termios;    // store clean copy of terminal attributes

/****************\
|*** terminal ***|
\****************/
void die(const char* s)
{
    perror(s);  // using perror for descriptiveness and ability to print the string before throwing error
    exit(1);
}

void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattrD");
}

void enableRawMode(void)
{
    /* read terminal attributes into orig_termios */
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr"); 

    /* register disableRawMode() to be called automatically when program exits*/
    atexit(disableRawMode); 

    /* declare variable raw of type struct termios and populate it with the clean copy */
    struct termios raw = orig_termios; 

    /* Disable ctrl s/q with IXON input flag. Fix ctrl m from reading as 10 with ICRNL. And other stuff. */    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Turn off output translation, NO MORE \r\n unless I SAY SO!!! */
    raw.c_oflag &= ~(OPOST);

    /* just in case */
    raw.c_cflag |= (CS8);

    /* Perform bitwise-NOT on echo (bitflag), then bitwise-AND with the flags field
     * to properly bit flip ECHO, then OR with ICANON to disable canonical mode. 
     * Use ISIG flag to disable ctrl c/z signals. 
     * Use IEXTEN flag to disable ctrl v. */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;     // read() requires 0 bytes to return, so it returns upon input
    raw.c_cc[VTIME] = 1;    // read() waits 1/10 of a second to return

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattrE");   // set new attributes from raw
}

/************\
|*** init ***|
\************/
int main(void)
{
    enableRawMode();

    while (1) {
        char c = '\0';     // declare char variable c to store input and init NULL
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");    // read input into c
        if (iscntrl(c)) {           /* from ctype.h--tests whether character is a control (0-31, 127) */

            printf("%d\r\n", c);
        } else {

            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }
    return 0;
}
