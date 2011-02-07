#include <fixpoint.h>
#include <assert.h>
#include <dac.h>
#include "abstract.h"

struct abstract abstract;

static const int16_t SINE [257] = {
	0,	804,	1608,	2410,
	3212,	4011,	4808,	5602,
	6393,	7179,	7962,	8739,
	9512,	10278,	11039,	11793,
	12539,	13279,	14010,	14732,
	15446,	16151,	16846,	17530,
	18204,	18868,	19519,	20159,
	20787,	21403,	22005,	22594,
	23170,	23731,	24279,	24811,
	25329,	25832,	26319,	26790,
	27245,	27683,	28105,	28510,
	28898,	29268,	29621,	29956,
	30273,	30571,	30852,	31113,
	31356,	31580,	31785,	31971,
	32137,	32285,	32412,	32521,
	32609,	32678,	32728,	32757,
	32767,	32757,	32728,	32678,
	32609,	32521,	32412,	32285,
	32137,	31971,	31785,	31580,
	31356,	31113,	30852,	30571,
	30273,	29956,	29621,	29268,
	28898,	28510,	28105,	27683,
	27245,	26790,	26319,	25832,
	25329,	24811,	24279,	23731,
	23170,	22594,	22005,	21403,
	20787,	20159,	19519,	18868,
	18204,	17530,	16846,	16151,
	15446,	14732,	14010,	13279,
	12539,	11793,	11039,	10278,
	9512,	8739,	7962,	7179,
	6393,	5602,	4808,	4011,
	3212,	2410,	1608,	804,
	0,	-804,	-1608,	-2410,
	-3212,	-4011,	-4808,	-5602,
	-6393,	-7179,	-7962,	-8739,
	-9512,	-10278,	-11039,	-11793,
	-12539,	-13279,	-14010,	-14732,
	-15446,	-16151,	-16846,	-17530,
	-18204,	-18868,	-19519,	-20159,
	-20787,	-21403,	-22005,	-22594,
	-23170,	-23731,	-24279,	-24811,
	-25329,	-25832,	-26319,	-26790,
	-27245,	-27683,	-28105,	-28510,
	-28898,	-29268,	-29621,	-29956,
	-30273,	-30571,	-30852,	-31113,
	-31356,	-31580,	-31785,	-31971,
	-32137,	-32285,	-32412,	-32521,
	-32609,	-32678,	-32728,	-32757,
	-32767,	-32757,	-32728,	-32678,
	-32609,	-32521,	-32412,	-32285,
	-32137,	-31971,	-31785,	-31580,
	-31356,	-31113,	-30852,	-30571,
	-30273,	-29956,	-29621,	-29268,
	-28898,	-28510,	-28105,	-27683,
	-27245,	-26790,	-26319,	-25832,
	-25329,	-24811,	-24279,	-23731,
	-23170,	-22594,	-22005,	-21403,
	-20787,	-20159,	-19519,	-18868,
	-18204,	-17530,	-16846,	-16151,
	-15446,	-14732,	-14010,	-13279,
	-12539,	-11793,	-11039,	-10278,
	-9512,	-8739,	-7962,	-7179,
	-6393,	-5602,	-4808,	-4011,
	-3212,	-2410,	-1608,	-804, 0 
};



// Used for sine and exponentials that cannot be quickly programatically generated
// phase 0 -> UINT32_TMAX = 0 ->2PI 
static inline fixed interpolatewave (const uint32_t phase, const int16_t * const waveform)
{
	ASSERT (waveform);
	int32_t v1, v2;
	const uint32_t index = (phase >> 24);// 0 -> 255
	const uint32_t frac = (phase & 0x00ffffff)>>8; // Avoids risking overflow
	int32_t delta;
	v1 = waveform[index];
	v2 = waveform[index+1];
	// note that this may = 256 hence why there are 257 values in the LUT
	delta = v2 - v1; 
	v1 = v1 << 16;
	delta *=frac;
	// frac is in 0 -> ffff, so this means that (delta >>8) + v1 = 32 bit result.
	return (v1+delta)>>INTEGER; // scale to +- 1.0
}

static inline fixed fix_sine (const uint32_t phase)
{
  return interpolatewave (phase,SINE);
}

static inline fixed fix_sawtooth (const uint32_t phase)
{
  return FIXED(-1) + (phase >> (INTEGER-1)); 
}

static inline fixed fix_triangle(const uint32_t phase)
{
  if (phase & 0x80000000){// second half of the cycle
    return FIXED(2) - (phase >> (INTEGER-1));
  } else {
    return FIXED(-1) + (phase >> (INTEGER-1));
  }
}

static inline fixed fix_square (const uint32_t phase)
{
  if (phase & 0x80000000){
    return FIXED(1);
  } else {
    return FIXED(-1);
  }
}


// An oscilator runs at one frequency and combines sine, saw, 
// triangle and square waves at arbitary levels and phases
static fixed oscilator_run (struct oscilator *osc)
{
  uint32_t phase;
  fixed accum;
  osc->phase += osc->increment;
  phase = osc->phase + osc->phase_offset;
  accum = fix_mul (osc->gains[0],fix_sine(phase + osc->phases[0]));
  accum += fix_mul (osc->gains[1],fix_sawtooth(phase + osc->phases[1])); 
  accum += fix_mul (osc->gains[2],fix_triangle(phase + osc->phases[2])); 
  accum += fix_mul (osc->gains[0],fix_square(phase + osc->phases[3]));
  return accum;
}


// The second and third oscilators can be added or can 
// amplitude modulate the first oscilator.
// TODO allow the first to phase modulate the others 
static fixed threeosc_run (struct threeosc * oscs)
{
  fixed accum;
  fixed intermed;
  accum = oscilator_run (&oscs->osc[0]);
  intermed = oscilator_run(&oscs->osc[1]);
  if (oscs->mode[0] & SUM) {
     accum += intermed;
  }
  if (oscs->mode[0] & AM){
    accum *= intermed + FIXED(1);
  }
  if (oscs->mode[0] & PRODUCT){
    accum *= intermed;
  }
  intermed = oscilator_run(&oscs->osc[2]);
  if (oscs->mode[1] & SUM) {
     accum += intermed;
  }
  if (oscs->mode[1] & AM){
    accum *= intermed + FIXED(1);
  }
  if (oscs->mode[1] & PRODUCT){
    accum *= intermed;
  }
  return accum;
}


dac_point_t abstract_run (struct abstract *ab)
{
  dac_point_t p;
  fixed z;
  fixed blanking;

  // point position
  z = FIXED(1) + (ab->z_scale * threeosc_run(&ab->axis[2]));
  p.x = fix_mul(z,threeosc_run(&ab->axis[0])) & 0xffff;
  p.y = fix_mul(z,threeosc_run(&ab->axis[1])) & 0xffff;
 
  // colour
  p.r = threeosc_run(&ab->colours[0]) & 0xffff;
  p.g = threeosc_run(&ab->colours[1]) & 0xffff;
  p.b = threeosc_run(&ab->colours[2]) & 0xffff;

  // blanker
  blanking = threeosc_run(&ab->blanker);
  if (blanking < ab->blanker_threshold) {
    p.r = p.g = p.b = 0;
  }
  // intensity
  p.i = (FIXED(0.3333) * FIXED((uint32_t)p.r + p.g + p.b))>>POINT;
  return p;

}
