#include <stdint.h>

typedef struct {
    float alpha;         // Smoothing factor
    float prev_input;    // Previous input sample
    float prev_output;   // Previous output sample
} HighPassFilter;

typedef struct {
    float alpha;         // Smoothing factor
    float y_prev;        // Previous output sample
} IIRLowPassFilter;

typedef struct {
    float out;
	float *coeff;
	float *buf;
	uint8_t order;
	uint8_t putIndex;
} FIRFilter ;

void FIRFilter_Init(FIRFilter *filt, float *coeff, float *buf, const uint8_t order);
float FIRFilter_Update(FIRFilter *filt, float in);

float apply_highpass_filter(HighPassFilter *hpf, float input);
void init_highpass_filter(HighPassFilter *hpf, float sampling_rate, float cutoff_frequency);
float apply_lowpass_filter(IIRLowPassFilter *lpf, float input);
void init_lowpass_filter(IIRLowPassFilter *lpf, float sampling_rate, float cutoff_frequency);