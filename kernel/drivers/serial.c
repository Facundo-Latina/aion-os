#include "../include/serial.h"
#include "../include/io.h"
#include <stdint.h>
#include <stdarg.h>
#define COM1 0x3F8
void serial_init(void) {
    outb(COM1+1,0x00); outb(COM1+3,0x80);
    outb(COM1+0,0x03); outb(COM1+1,0x00);
    outb(COM1+3,0x03); outb(COM1+2,0xC7);
    outb(COM1+4,0x0B);
}
static int serial_tx_ready(void) { return inb(COM1+5) & 0x20; }
void serial_putc(char c) {
    while (!serial_tx_ready());
    outb(COM1, c);
    if (c=='\n') { while(!serial_tx_ready()); outb(COM1,'\r'); }
}
void serial_puts(const char *s) { while(*s) serial_putc(*s++); }
static void print_uint(uint64_t v, int base) {
    char buf[64]; int i=0;
    if(!v){serial_putc('0');return;}
    while(v){buf[i++]="0123456789abcdef"[v%base];v/=base;}
    while(i-->0) serial_putc(buf[i]);
}
int serial_printf(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt);
    for(;*fmt;fmt++){
        if(*fmt!='%'){serial_putc(*fmt);continue;}
        fmt++;
        switch(*fmt){
            case 's': serial_puts(va_arg(ap,char*)); break;
            case 'd': {long long v=va_arg(ap,long long); if(v<0){serial_putc('-');v=-v;} print_uint(v,10);} break;
            case 'u': print_uint(va_arg(ap,unsigned long long),10); break;
            case 'x': case 'X': print_uint(va_arg(ap,unsigned long long),16); break;
            case 'l': fmt++; if(*fmt=='l') fmt++;
                      if(*fmt=='u') print_uint(va_arg(ap,unsigned long long),10);
                      else if(*fmt=='d'){long long v=va_arg(ap,long long);if(v<0){serial_putc('-');v=-v;}print_uint(v,10);}
                      break;
            case 'c': serial_putc((char)va_arg(ap,int)); break;
            case '%': serial_putc('%'); break;
        }
    }
    va_end(ap);
    return 0;
}
