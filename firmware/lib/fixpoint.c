#include <assert.h>
#include "fixpoint.h"

#define ITERS  (15 + (POINT >>1))

fixed fix_sqrt(const fixed x)
{
  uint32_t root, remHi, remLo, testDiv, count;
  root = remHi  = 0;
  remLo = x;
  count = ITERS;
  ASSERT(x >= 0);
  do {
    remHi = (remHi << 2) | (remLo >> 30); 
    remLo <<= 2; /* get 2 bits of arg */
    root <<= 1;
    /* Get ready for the next bit in the root */
    testDiv = (root << 1) + 1;
    /* Test radical */
    if (remHi >= testDiv) {
      remHi -= testDiv;
      root += 1;
    }
  } while (count--);
  return(root);
}

#define AG_CONST  (0.6072529350)

/// NOTE fix_cordic expects theta to be in radians between -Pi/2 and Pi/2 
/// Returns sine of angle
fixed fix_cordic (const fixed theta, fixed *s, fixed *c)
{
  // Table values are based on arctan (1 * 2^(-n)) so 1,.5,.25,.125,,,,
  static const fixed angles[16]= {
    FIXED(0.78539816),FIXED(0.46364761),
    FIXED(0.24497866),FIXED(0.12435499),
    FIXED(0.06241881),FIXED(0.03123983),
    FIXED(0.01562373),FIXED(0.00781234),
    FIXED(0.00390623),FIXED(0.00195312),
    FIXED(0.00097656),FIXED(0.00048828),
    FIXED(0.00024414),FIXED(0.00012207),
    FIXED(0.00006104),FIXED(0.00003052),
  };
  fixed x, y, curr_angle;
  unsigned int step;

  x = FIXED(AG_CONST);  /* AG_CONST * cos(0) */
  y = 0;        /* AG_CONST * sin(0) */
  curr_angle = 0;
  for(step = 0; step < 16; step++){  
    fixed new_x;
    if(theta > curr_angle){  
      new_x = x - (y >> step);
      y = (x >> step) + y;
      x=new_x;
      curr_angle += angles[step]; 
    } else {  
      new_x=x + (y >> step);
      y = -(x >> step) + y;
      x = new_x;
      curr_angle -= angles[step]; 
    }
  }
  if (s) {
    *s = y;
  }
  if (c){
    *c = x;
  }
  return y;
}

#ifdef TEST

#include <stdio.h>

int main (void)
{
  fixed x,y;
  fixed r;
  x = FIXED(100);
  y = FIXED(123.456);
  r = fix_mul (x,y);
  printf ("product of %f * %f = %f\n",FLOAT(x),FLOAT(y),FLOAT(r));
  x = FIXED(-1.23);
  r = fix_mul (x,y);
  printf ("product of %f * %f = %f\n",FLOAT(x),FLOAT(y),FLOAT(r));
  y = FIXED(-1.23);
  r = fix_mul (x,y);
  printf ("product of %f * %f = %f\n",FLOAT(x),FLOAT(y),FLOAT(r));
  y = FIXED(100);
  r = fix_sqrt (y);
  printf ("root of %f = %f\n",FLOAT(y),FLOAT(r));
  y = FIXED(0);
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));
  y = FIXED(0);
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));
  y = FIX_PI/2;
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));
  y = FIX_PI/-2;
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));
  y = FIX_PI/4;
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));
  y = FIX_PI/-4;
  printf ("sin of %f = %f\n",FLOAT(y),FLOAT(fix_cordic(y,NULL,NULL)));

  return 0;
}

#endif
