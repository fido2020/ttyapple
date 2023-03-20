#include <stdio.h>

#if defined(__unix)

#include <unistd.h>
void us_sleep(long us) {
    usleep(us);
}

void reset_cursor() {
    puts("\033[1;1H");
}

#elif defined(WIN32)

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
void us_sleep(long us) {
    sleep(x / 1000)
}

HANDLE stdoutHandle;
COORD cursorTopLeft = {0, 0};

void reset_cursor() {
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cursorTopLeft);
}

#else

void us_sleep(long us) {}

void reset_cursor() {
    puts("\033[1;1H");
}

#endif

void print_text(char* text) {
    puts(text);
}

void play_frames();

int main() {
    play_frames();
}
