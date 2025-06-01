#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

struct termios orig_termios;    // store clean copy of terminal attributes

void disableRawMode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);  // put back original term attributes
}

void enableRawMode(void)
{
    /* read terminal attributes into orig_termios */
    tcgetattr(STDIN_FILENO, &orig_termios); 

    /* register disableRawMode() to be called automatically when program exits*/
    atexit(disableRawMode); 

    /* declare variable raw of type struct termios and populate it with the clean copy */
    struct termios raw = orig_termios; 

    // perform bitwise-NOT on echo (bitflag), then bitwise-AND with the flags field
    raw.c_lflag &= ~(ECHO);     // disable echo

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);   // set new attributes from raw
}

int main(void)
{
    enableRawMode();

    char c;     // declare char variable c to store input
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');     // read input into c
    return 0;
}
