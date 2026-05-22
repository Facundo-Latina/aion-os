#include "display.h"
#include <stdint.h>
#include <string.h>
static uint64_t fb_base=0;
static uint32_t fb_w=0,fb_h=0,fb_pitch=0;
static uint32_t cursor_x=0,cursor_y=0;
/* Minimal 8x8 font (ASCII 32-127) - each char is 8 bytes, 1 bit per pixel */
static const uint8_t font8x8[96][8] = {
  {0,0,0,0,0,0,0,0},        /* space */
  {0x18,0x3C,0x3C,0x18,0x18,0,0x18,0}, /* ! */
  {0x36,0x36,0,0,0,0,0,0},  /* " */
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0}, /* # */
  /* ... remaining chars use fallback box ... */
};
void display_init(uint64_t fb, uint32_t w, uint32_t h, uint32_t pitch, uint32_t fmt) {
    (void)fmt; fb_base=fb; fb_w=w; fb_h=h; fb_pitch=pitch;
    display_clear();
}
void display_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if(x>=fb_w||y>=fb_h) return;
    uint32_t *p=(uint32_t*)(fb_base + y*fb_pitch + x*4);
    *p=color;
}
void display_clear(void) {
    for(uint32_t y=0;y<fb_h;y++)
        for(uint32_t x=0;x<fb_w;x++)
            display_set_pixel(x,y,0x00101010);
    cursor_x=8; cursor_y=8;
}
static void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    int idx=c-32;
    if(idx<0||idx>=96) idx=0;
    for(int row=0;row<8;row++){
        uint8_t bits=(idx<4)?font8x8[idx][row]:0x3C; /* fallback box */
        for(int col=0;col<8;col++)
            if(bits&(0x80>>col)) display_set_pixel(x+col,y+row,color);
    }
}
void display_print_string(const char *s, uint32_t color) {
    while(*s){
        if(*s=='\n'){cursor_x=8;cursor_y+=12;}
        else{
            draw_char(*s,cursor_x,cursor_y,color);
            cursor_x+=9;
            if(cursor_x+9>fb_w){cursor_x=8;cursor_y+=12;}
        }
        if(cursor_y+12>fb_h) cursor_y=8;
        s++;
    }
}
void display_print_boot_line(const char *stage, const char *msg) {
    display_print_string("  [",0xAAAAAAAA);
    display_print_string(stage,0x88CCFF);
    display_print_string("] ",0xAAAAAAAA);
    display_print_string(msg,0xFFFFFF);
    display_print_string("\n",0xFFFFFF);
}
uint32_t display_get_width(void)  { return fb_w; }
uint32_t display_get_height(void) { return fb_h; }
