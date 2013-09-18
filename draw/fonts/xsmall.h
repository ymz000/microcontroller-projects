#ifndef FONT_XSMALL_H
#define FONT_XSMALL_H

#include <avr/io.h>
#include <avr/pgmspace.h> 

// Workaround for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34734
#ifdef PROGMEM
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))
#endif

extern uint8_t font_xsmall[];

#define FONT_XSMALL_WIDTH		3
#define FONT_XSMALL_HEIGHT		5

#endif