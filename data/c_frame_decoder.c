#include <stdint.h>

#if defined(WIN32)

#define ENCODING_CP437

#elif defined(UEFI)

#define ENCODING_UTF16

#endif

#define FRAME_STRIDE ((FRAME_WIDTH + 7) / 8)

#if (FRAME_HEIGHT % 2)
    #error "FRAME_HEIGHT must be divisible by 2!"
#endif

#if defined(ENCODING_CP437)

typedef char tty_char_t;

#define CP437_BLOCK_CHARACTER 0xDB
#define CP437_HALFBLOCK_TOP 0xDF
#define CP437_HALFBLOCK_BOTTOM 0xDC

#define put_block_character(text, i) text[i++] = CP437_BLOCK_CHARACTER;
#define put_block_top_character(text, i) text[i++] = CP437_HALFBLOCK_TOP;
#define put_block_bottom_character(text, i) text[i++] = CP437_HALFBLOCK_BOTTOM;
#define put_blank_character(text, i) text[i++] = ' ';
#define put_line_ending(text, i) text[i++] = '\r'; text[i++] = '\n';

#elif defined(ENCODING_UTF16)

typedef uint16_t tty_char_t;

#define UTF16_BLOCK 0x2588
#define UTF16_HALFBLOCK_TOP 0x2580
#define UTF16_HALFBLOCK_BOTTOM 0x2584

#define put_block_character(text, i) text[i++] = UTF16_BLOCK;
#define put_block_top_character(text, i) text[i++] = UTF16_HALFBLOCK_TOP;
#define put_block_bottom_character(text, i) text[i++] = UTF16_HALFBLOCK_BOTTOM;
#define put_blank_character(text, i) text[i++] = ' ';
#define put_line_ending(text, i) text[i++] = '\r'; text[i++] = '\n';

#else

typedef char tty_char_t;

#define UTF8_BLOCK_0 0xE2
#define UTF8_BLOCK_1 0x96

#define UTF8_BLOCK_2 0x88
#define UTF8_HALFBLOCK_TOP_2 0x80
#define UTF8_HALFBLOCK_BOTTOM_2 0x84

#define put_block_character(text, i) text[i++] = UTF8_BLOCK_0; \
    text[i++] = UTF8_BLOCK_1; text[i++] = UTF8_BLOCK_2;
#define put_block_top_character(text, i) text[i++] = UTF8_BLOCK_0; \
    text[i++] = UTF8_BLOCK_1; text[i++] = UTF8_HALFBLOCK_TOP_2;
#define put_block_bottom_character(text, i) text[i++] = UTF8_BLOCK_0; \
    text[i++] = UTF8_BLOCK_1; text[i++] = UTF8_HALFBLOCK_BOTTOM_2;
#define put_blank_character(text, i) text[i++] = ' ';
#define put_line_ending(text, i) text[i++] = '\n';

#endif

void us_sleep(long us);
void reset_cursor();
void print_text(tty_char_t* text);

// Very lazy but let's just multiply the frame width by 3 to account
// for the unicode characters.
tty_char_t text[(FRAME_WIDTH * 3 + 1) * (FRAME_HEIGHT / 2) + 1];

void play_frames() {
    int framesLeft = FRAME_COUNT;

    uint8_t** _frames = frames;
    while(framesLeft--) {
        uint8_t* frame = *(_frames++);
        
        int i = 0;
        for(int row = 0; row < FRAME_HEIGHT; row += 2) {
            uint8_t* top = frame + FRAME_STRIDE * row;
            uint8_t* bottom = frame + FRAME_STRIDE * (row + 1);
            
            int c = 0;
            while(c < FRAME_WIDTH) {
                uint8_t p1 = *(top++);
                uint8_t p2 = *(bottom++);

                int p = 7;
                if(p > FRAME_WIDTH - c - 1) {
                    p = 7 - (FRAME_WIDTH - c - 1);
                }

                c += (p + 1);

                while(p >= 0) {
                    if(((p1 >> p) & 1)) {
                        if((p2 >> p) & 1) {
                            put_block_character(text, i);
                        } else {
                            put_block_top_character(text, i);
                        }
                    } else if((p2 >> p) & 1) {
                        put_block_bottom_character(text, i);
                    } else {
                        put_blank_character(text, i);
                    }

                    p--;
                }
            }

            // End the line
            put_line_ending(text, i);
        }

        text[i++] = 0;

        reset_cursor();
        print_text(text);

        us_sleep(FRAME_INTERVAL);
    }
}
