/*
 * adc_serivce.h
 *
 * Created: 4/17/2024 11:28:03 AM
 *  Author: classicComputer
 */ 


#ifndef ADC_SERIVCE_H_
#define ADC_SERIVCE_H_

#include "adc.h"


#define NUM_CHANNELS 8
/** ADC convention done mask. */
#define ADC_DONE_MASK   ( (1<<NUM_CHANNELS) - 1 )
/** The maximal digital value */
#define MAX_DIGITAL     (4096)
/** Reference voltage for ADC, in mv. */
#define VOLT_REF        (3300)
/** ADC sample data */
struct {
	uint8_t uc_ch_num[NUM_CHANNELS];
	uint16_t us_value[NUM_CHANNELS];
	uint16_t us_done;
} g_adc_sample_data;

void initializeAdc(void);

#endif /* ADC_SERIVCE_H_ */