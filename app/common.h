
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include "stm32f4xx.h"

#ifdef MAIN
#define GBLDEF(name,value)        name = value
#else
#define GBLDEF(name,value)        extern name
#endif

#ifdef DEBUG
#define EPRINTF(a) printf a
#define DPRINTF(a) printf a
#define FPRINTF(a) printf a

#define assert(ex)  do {                                               \
                          if (!(ex)) {                                 \
	                      (void)printf("Assertion failed: file \"%s\", \
    	                  line %d\n", __FILE__, __LINE__);             \
                          }                                            \
                        } while (0)

#else
#define FPRINTF(a) printf a
#define EPRINTF(a)
#define DPRINTF(a)

#define assert(ex)
#endif

#ifndef UNSUED
#define UNUSED(x) ((void)(x))
#endif

#define MAX3(a,b,c) (a>b?(a>c?a:c):(b>c?b:c))

#define DisableMIRQ __asm volatile ( " cpsid i " ::: "memory")
#define EnableMIRQ  __asm volatile ( " cpsie i " ::: "memory")

typedef int64_t ticks_t;

#define MS_TO_TICKS(a)  (a)

typedef void (*pFunction)(void);

inline unsigned bswap16(unsigned x)
{
    __asm__("rev16 %0, %0" : "+r"(x));
    return x;
}


inline uint32_t bswap32(uint32_t x)
{
    __asm__("rev %0, %0" : "+r"(x));
    return x;
}


#endif

