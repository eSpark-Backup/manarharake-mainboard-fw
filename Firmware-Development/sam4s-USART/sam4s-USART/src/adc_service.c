/*
 * adc_service.c
 *
 * Created: 4/17/2024 11:31:12 AM
 *  Author: classicComputer
 */ 

#include "adc_serivce.h"

#include "pmc.h"
#include "sysclk.h"
#include "string.h"

enum adc_channel_num_t ch_list[NUM_CHANNELS] = {
	ADC_CHANNEL_0,
	ADC_CHANNEL_1,
	ADC_CHANNEL_2,
	ADC_CHANNEL_3,
	ADC_CHANNEL_4,
	ADC_CHANNEL_5,
	ADC_CHANNEL_6,
	ADC_CHANNEL_7
};

/* Setup ADC Configurations */
void initializeAdc(void)
{
	pmc_enable_periph_clk(ID_ADC);
	adc_init(ADC, sysclk_get_cpu_hz(), 6400000, ADC_STARTUP_TIME_4);
	memset((void *)&g_adc_sample_data, 0, sizeof(g_adc_sample_data));

	adc_configure_timing(ADC, 0, ADC_SETTLING_TIME_3, 1);
	
	adc_enable_tag(ADC);
	adc_configure_sequence(ADC, ch_list, NUM_CHANNELS);

	/* Enable sequencer. */
	adc_start_sequencer(ADC);
	
	/* Enable channels. */
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		adc_enable_channel(ADC, (enum adc_channel_num_t)i);
		// 		adc_set_channel_input_gain(ADC, (enum adc_channel_num_t)i, ADC_GAINVALUE_1);
		// 		adc_disable_channel_differential_input(ADC, (enum adc_channel_num_t)i);
		/* Update channel number. */
		g_adc_sample_data.uc_ch_num[i] = ch_list[i];
	}
	
	adc_set_resolution(ADC, ADC_MR_LOWRES_BITS_12);

	adc_enable_interrupt(ADC, ADC_IER_DRDY);
	NVIC_EnableIRQ(ADC_IRQn);
	
	adc_configure_trigger(ADC, ADC_TRIG_SW, ADC_MR_FREERUN_ON);
	
}

/************************************************************************/
/*							ADC HANDLER									*/
/************************************************************************/
void ADC_Handler(void)
{
	uint32_t i;
	uint32_t ul_temp;
	uint8_t uc_ch_num;
	// Check the ADC conversion status
	if ((adc_get_status(ADC) & ADC_ISR_DRDY) == ADC_ISR_DRDY)
	{
		ul_temp = adc_get_latest_value(ADC);
		for (i = 0; i < NUM_CHANNELS; i++) {
			uc_ch_num = (ul_temp & ADC_LCDR_CHNB_Msk) >> ADC_LCDR_CHNB_Pos;
			if (g_adc_sample_data.uc_ch_num[i] == uc_ch_num) {
				g_adc_sample_data.us_value[i] =	ul_temp & ADC_LCDR_LDATA_Msk;
				g_adc_sample_data.us_done |= 1 << i;
			}
		}
		//result = adc_get_latest_value(ADC);
	}
	
}