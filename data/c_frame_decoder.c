#include <stdio.h>
#include <stdint.h>

#ifdef __unix

#include <unistd.h>

#define us_sleep(x) usleep(x)

#elif WIN32

#include <windows.h>
#define us_sleep(x) Sleep(x / 1000)

#else

// We're shit out of luck
#define us_sleep(x)

#endif

#include "frames.h"

#define FRAME_INTERVAL (1000000 / 24)

#define FRAME_WIDTH 96
#define FRAME_HEIGHT 72

#define FRAME_STRIDE ((FRAME_WIDTH + 7) / 8)

#if (FRAME_HEIGHT % 2)
    #error "FRAME_HEIGHT must be divisible by 2!"
#endif

#define UTF8_BLOCK {0xE2,0x96,0x88}
#define UTF8_HALFBLOCK_TOP {0xE2, 0x96, 0x80}
#define UTF8_HALFBLOCK_BOTTOM {0xE2, 0x96, 0x84}

#define CP437_BLOCK_CHARACTER {0xDB}
#define CP437_HALFBLOCK_TOP {0xDF}
#define CP437_HALFBLOCK_BOTTOM {0xDC}

int main() {
    int framesLeft = FRAME_COUNT;

    // Very lazy but let's just multiply the frame width by 3 to account
    // for the unicode characters.
    char text[(FRAME_WIDTH * 3 + 1) * (FRAME_HEIGHT / 2) + 1];

    const char block[] = UTF8_BLOCK;
    const char topBlock[] = UTF8_HALFBLOCK_TOP;
    const char bottomBlock[] = UTF8_HALFBLOCK_BOTTOM;

    while(framesLeft--) {
        uint8_t* frame = *(frames++);
        
        int i = 0;
        for(int row = 0; row < FRAME_HEIGHT; row += 2) {
            uint8_t* top = frame + FRAME_STRIDE * row;
            uint8_t* bottom = frame + FRAME_STRIDE * (row + 1);
            
            int c = 0;
            while(c < FRAME_WIDTH) {
                uint8_t p1 = *(top++);
                uint8_t p2 = *(bottom++);

                int p = 7;
                if(p > FRAME_WIDTH - i - 1) {
                    p = 7 - (FRAME_WIDTH - i - 1);
                }

                while(p >= 0) {
                    if(((p1 >> p) & 1)) {
                        if((p2 >> p) & 1) {
                            text[i++] = block[0];
                            text[i++] = block[1];
                            text[i++] = block[2];
                            text[i++] = topBlock[1];
                            text[i++] = topBlock[2];
                        }
                    } else if((p2 >> p) & 1) {
                        text[i++] = bottomBlock[0];
                        text[i++] = bottomBlock[1];
                        text[i++] = bottomBlock[2];
                    } else {
                        text[i] = ' ';
                    }

                    c++;
                }
            }

            // End the line
            text[i] = '\n';
        }

        text[i] = 0;
        puts(text);

        us_sleep(FRAME_INTERVAL);
    }

    return 0;
}
