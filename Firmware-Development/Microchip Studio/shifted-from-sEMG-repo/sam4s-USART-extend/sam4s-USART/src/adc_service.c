/*
 * adc_service.c
 *
 * Created: 4/17/2024 11:31:12 AM
 *  Author: classicComputer
 */ 

#include "adc_serivce.h"
#include <asf.h>
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

static uint32_t adc_read_buffer(Adc * p_adc, uint16_t * p_s_buffer, uint32_t ul_size)
{
	/* Check if the first PDC bank is free. */
	if ((p_adc->ADC_RCR == 0) && (p_adc->ADC_RNCR == 0)) {
		p_adc->ADC_RPR = (uint32_t) p_s_buffer;
		p_adc->ADC_RCR = ul_size;
		p_adc->ADC_PTCR = ADC_PTCR_RXTEN;

		return 1;
	} else { /* Check if the second PDC bank is free. */
		if (p_adc->ADC_RNCR == 0) {
			p_adc->ADC_RNPR = (uint32_t) p_s_buffer;
			p_adc->ADC_RNCR = ul_size;

			return 1;
		} else {
			return 0;
		}
	}
}

static void configure_time_trigger(void)
{
	uint32_t ul_div = 0;
	uint32_t ul_tc_clks = 0;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	/* Enable peripheral clock. */
	pmc_enable_periph_clk(ID_TC0);

	/* TIOA configuration */
	gpio_configure_pin(PIN_TC0_TIOA0, PIN_TC0_TIOA0_FLAGS);

	
	/* Configure TC for a 1Hz frequency and trigger on RC compare. */
	tc_find_mck_divisor(TIMER_ADC_SAMPLING_RATE, ul_sysclk, &ul_div, &ul_tc_clks, ul_sysclk);
	tc_init(TC0, 0, ul_tc_clks | TC_CMR_CPCTRG | TC_CMR_WAVE |
			TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET);
	TC0->TC_CHANNEL[0].TC_RA = (ul_sysclk / ul_div) / 2;
	TC0->TC_CHANNEL[0].TC_RC = (ul_sysclk / ul_div) / TIMER_ADC_SAMPLING_RATE;

	/* Start the Timer. */
	tc_start(TC0, 0);
	/* Set TIOA0 trigger. */
#if SAM3S || SAM3N || SAM3XA || SAM4S || SAM4C
	adc_configure_trigger(ADC, ADC_TRIG_TIO_CH_0, 1);
#elif SAM3U
#ifdef ADC_12B
	adc12b_configure_trigger(ADC12B, ADC12B_TRIG_TIO_CH_0);
#else
	adc_configure_trigger(ADC, ADC_TRIG_TIO_CH_0);
#endif
#endif
}

/* Setup ADC Configurations */
void initializeAdc(void)
{
	pmc_enable_periph_clk(ID_ADC);
	adc_init(ADC, sysclk_get_cpu_hz(), 6400000, ADC_STARTUP_TIME_1);
	memset((void *)&g_adc_sample_data, 0, sizeof(g_adc_sample_data));

	adc_configure_timing(ADC, TRACKING_TIME, ADC_SETTLING_TIME_0, TRANSFER_PERIOD);
	
	adc_enable_tag(ADC);
	adc_configure_sequence(ADC, ch_list, NUM_CHANNELS);

	/* Enable sequencer. */
	adc_start_sequencer(ADC);
	
	adc_disable_anch(ADC);
	// adc_enable_anch(ADC);

	/* Enable channels. */
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		adc_enable_channel(ADC, (enum adc_channel_num_t)i);
		// 		adc_set_channel_input_gain(ADC, (enum adc_channel_num_t)i, ADC_GAINVALUE_1);
		// 		adc_disable_channel_differential_input(ADC, (enum adc_channel_num_t)i);
		/* Update channel number. */
		g_adc_sample_data.uc_ch_num[i] = ch_list[i];

		/* gain = 1 */
		adc_set_channel_input_gain(ADC, ch_list[i], ADC_GAINVALUE_0);

		adc_disable_channel_input_offset(ADC, ch_list[i]);
	}

	adc_set_calibmode(ADC);
	while (1) {
		if ((adc_get_status(ADC) & ADC_ISR_EOCAL) ==
				ADC_ISR_EOCAL)
			break;
	}

	adc_configure_power_save(ADC, 0, 0);

	adc_read_buffer(ADC, g_adc_sample_data.us_value, ADC_PDC_BUFFER_SIZE);
		/* Enable PDC channel interrupt. */
	adc_enable_interrupt(ADC, ADC_IER_RXBUFF);
	
	adc_set_resolution(ADC, ADC_MR_LOWRES_BITS_12);

	// adc_enable_interrupt(ADC, ADC_IER_DRDY);
	NVIC_EnableIRQ(ADC_IRQn);
	
	configure_time_trigger();
	// adc_configure_trigger(ADC, ADC_TRIG_TIO_CH_0, 0);
	
}

/************************************************************************/
/*							ADC HANDLER									*/
/************************************************************************/
void ADC_Handler(void)
{
	uint32_t i;
	uint32_t ul_temp;
	uint8_t uc_ch_num;

	/* PDC Sequence */
	if ((adc_get_status(ADC) & ADC_ISR_RXBUFF) == ADC_ISR_RXBUFF) {
		g_adc_sample_data.us_done = ADC_DONE_MASK;
		adc_read_buffer(ADC, g_adc_sample_data.us_value, ADC_PDC_BUFFER_SIZE);
		/* Only keep sample value, and discard channel number. */
		for (i = 0; i < NUM_CHANNELS; i++) {
			g_adc_sample_data.us_value[i] &= ADC_LCDR_LDATA_Msk;
		}
	}	
	
}