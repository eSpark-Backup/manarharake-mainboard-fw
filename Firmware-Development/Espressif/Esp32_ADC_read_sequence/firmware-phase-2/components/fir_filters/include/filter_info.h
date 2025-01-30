/*

sampling frequency: 1000 Hz

* 0 Hz - 20 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = -27.722866754287722 dB

* 30 Hz - 300 Hz
  gain = 10
  desired ripple = 5 dB
  actual ripple = 36.179332465529676 dB

* 400 Hz - 500 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = -27.722866754287722 dB

*/

#define FILTER_TAP_NUM 16

static float filter_taps[FILTER_TAP_NUM] = {
  1.027608367821492,
  1.4697441710091166,
  -1.5381969387865588,
  -2.9290292594173954,
  -0.4631423600324384,
  -1.3020573424582795,
  -1.2002817867490576,
  4.914804446380787,
  4.914804446380787,
  -1.2002817867490576,
  -1.3020573424582795,
  -0.4631423600324384,
  -2.9290292594173954,
  -1.5381969387865588,
  1.4697441710091166,
  1.027608367821492
};
