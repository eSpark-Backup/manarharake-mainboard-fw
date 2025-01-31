#include "fir_filters.h"
#include "math.h"

float apply_highpass_filter(HighPassFilter *hpf, float input) 
{
    float output = hpf->alpha * (hpf->prev_output + input - hpf->prev_input);
    hpf->prev_input = input;     // Update previous input
    hpf->prev_output = output;   // Update previous output
    return output;               // Return the filtered value
}

void init_highpass_filter(HighPassFilter *hpf, float sampling_rate, float cutoff_frequency) 
{
    float dt = 1.0f / sampling_rate;
    float rc = 1.0f / (2.0f * M_PI * cutoff_frequency);
    hpf->alpha = rc / (rc + dt);
    hpf->prev_input = 0.0f;      // Initialize previous input to 0
    hpf->prev_output = 0.0f;     // Initialize previous output to 0
}

float apply_lowpass_filter(IIRLowPassFilter *lpf, float input) 
{
    float output = lpf->alpha * input + (1.0f - lpf->alpha) * lpf->y_prev;
    lpf->y_prev = output;        // Update previous output
    return output;               // Return the filtered value
}

void init_lowpass_filter(IIRLowPassFilter *lpf, float sampling_rate, float cutoff_frequency) 
{
    float dt = 1.0f / sampling_rate;
    float rc = 1.0f / (2.0f * M_PI * cutoff_frequency);
    lpf->alpha = dt / (rc + dt);
    lpf->y_prev = 0.0f;          // Initialize previous output to 0
}

void FIRFilter_Init(FIRFilter *filt, float *coeff, float *buf, const uint8_t order) {
	filt->out = 0.0f;
	
	filt->coeff = coeff;
	filt->buf   = buf;
	filt->order = order;
	filt->putIndex = 0;

	for (uint8_t n = 0; n < order; n++) {
		filt->buf[n] = 0.0f;
	}
}

float FIRFilter_Update(FIRFilter *filt, float in) {
    /* Store newest input value in circular buffer */
    filt->buf[filt->putIndex] = in;
    
    /* Compute filter output */
    uint8_t getIndex = filt->putIndex;
    
    filt->out = 0.0f;
    for (uint8_t n = 0; n < filt->order; n++) {
        filt->out = filt->out + filt->coeff[n] * filt->buf[getIndex];
        
        if (getIndex == 0) {
            getIndex = filt->order - 1;
        } else {
           getIndex--; 
        }
    }
    
    /* Increment buffer index */
    filt->putIndex++;
    if (filt->putIndex == filt->order) {
        filt->putIndex = 0;
    }

    // printf("%.2f, %.2f", filt->out, in);    

    /* Return output */
    return filt->out;
}