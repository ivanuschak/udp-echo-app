#include <termios.h>
#include <stdio.h>

static struct termios old, new;

static void initTermios(int echo) {
    tcgetattr(0, &old);
    new = old;
    new.c_lflag &= ~ICANON;
    new.c_lflag &= echo ? ECHO : ~ECHO;
    tcsetattr(0, TCSANOW, &new);
}

static void resetTermios(void) {
    tcsetattr(0, TCSANOW, &old);
}

static char getch_(int echo) {
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

char getch(void) {
    return getch_(0);
}

char getche(void) {
    return getch_(1);
}

