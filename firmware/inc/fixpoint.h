#ifndef FIXPOINT_INC
#define FIXPOINT_INC

#include <stdint.h>
#include <assert.h>

// 16.16 format fixed point
#define POINT (16)
#define INTEGER (16)
typedef int32_t fixed;

#define FIXED(x) ((fixed)((x) * (1<<POINT)))
#define FLOAT(x) ((float)(x)/(1<<POINT))
 
#define FIX_PI FIXED(3.1415926535)
#define FIX_E FIXED(2.718281828)

// use when one of the arguments is known to be |x|  < FIX(1.0)
static __attribute__((always_inline)) fixed fix_mul_small (const fixed x, const fixed y)
{
  return ((x * y) >> POINT);
}
/// general fixed point multiply 
static __attribute__((always_inline)) fixed fix_mul (const fixed x, const fixed y)
{
  return ((fixed)((((int64_t) x) * y)>> POINT));
}

static __attribute__((always_inline)) fixed fix_div (const fixed numerator, const fixed denom)
{
  ASSERT (denom !=0);
  return ((((int64_t)numerator)<<POINT)/denom);
}

fixed fix_sqrt (const fixed x);
fixed fix_cordic (const fixed theta, fixed *s, fixed *c);

#endif
