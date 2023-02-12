/* ISC license. */

#ifndef S6PS_INTERNAL_H
#define S6PS_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

typedef struct dius_s dius_t, *dius_t_ref ;
struct dius_s
{
  uint32_t left ;
  size_t right ;
} ;
#define DIUS_ZERO { .left = 0, .right = 0 }

#endif
