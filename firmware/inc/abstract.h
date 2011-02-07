#ifndef ABSTRACT_INC
#define ABSTRACT_INC

// A single oscilator
// combines arbitary amounts of sine, sawtooth, 
// triange and square at arbitary phases
struct oscilator 
{
  uint32_t phase;
  uint32_t increment;
  uint32_t phase_offset;
  fixed gains[4];
  uint32_t phases[4];
};


// Three of the above mixed in any combination  of three ways
enum OSC_MIXMODE {SUM=1,PRODUCT=2,AM=4};

struct threeosc
{
  struct oscilator osc[3];
  enum OSC_MIXMODE  mode[2];
};

// 19 oscilators each with 4 waveforms plus variable 
// blanking and z axis projection
struct abstract
{
  struct threeosc axis[3];
  struct threeosc colours[3];
  struct threeosc blanker;
  fixed blanker_threshold;
  fixed z_scale;
};
// The business end of the thing
dac_point_t abstract_run (struct abstract * const ab);



#endif
