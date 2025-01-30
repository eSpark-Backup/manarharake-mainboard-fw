#include <asf.h>

#include "trace_util/usb_initializer.h"
#include "adc.h"
#include <string.h>

/* Tracking Time*/
#define TRACKING_TIME         1
/* Transfer Period */
#define TRANSFER_PERIOD       1

/************************************************************************/
/*                           Prototypes                                 */
/************************************************************************/
void println(const char* str);
void receivetransmit(void);
static uint32_t adc_read_buffer(Adc * p_adc, uint16_t * p_s_buffer, uint32_t ul_size);
static void configure_time_trigger(void);
static void configure_pwm_trigger(void);
void start_adc(void);

/** ADC convention done mask. */
#define ADC_DONE_MASK   ( (1<<NUM_CHANNELS) - 1 )
#define NUM_CHANNELS 8
#define BUFFER_SIZE NUM_CHANNELS
#define VOLT_REF        (3300)
#define MAX_DIGITAL     (4095)
/** ADC sample data */
struct {
	uint8_t uc_ch_num[NUM_CHANNELS];
	uint16_t us_value[NUM_CHANNELS];
	uint16_t us_done;
} g_adc_sample_data;

/** ADC test mode structure */
struct {
	uint8_t uc_trigger_mode;
	uint8_t uc_pdc_en;
	uint8_t uc_sequence_en;
	uint8_t uc_gain_en;
	uint8_t uc_offset_en;
	uint8_t uc_power_save_en;
	uint8_t uc_auto_calib_en;
} g_adc_test_mode;

/**Channel list for sequence*/
enum adc_channel_num_t ch_list[8] = {
	ADC_CHANNEL_0,
	ADC_CHANNEL_1,
	ADC_CHANNEL_2,
	ADC_CHANNEL_3,
	ADC_CHANNEL_4,
	ADC_CHANNEL_5,
	ADC_CHANNEL_6,
	ADC_CHANNEL_7,
};

/** ADC trigger modes */
enum {
	TRIGGER_MODE_SOFTWARE = 0,
	TRIGGER_MODE_ADTRG,
	TRIGGER_MODE_TIMER,
	TRIGGER_MODE_PWM,
	TRIGGER_MODE_FREERUN
} e_trigger_mode;

/**
 *  Wait for the given number of milliseconds (using the dwTimeStamp generated
 *  by the SAM microcontrollers' system tick).
 *  \param ul_dly_ticks  Delay to wait for, in milliseconds.
 */
static volatile uint32_t gs_ul_ms_ticks = 0;
static void mdelay(uint32_t ul_dly_ticks)
{
	uint32_t ul_cur_ticks;

	ul_cur_ticks = gs_ul_ms_ticks;
	while ((gs_ul_ms_ticks - ul_cur_ticks) < ul_dly_ticks);
}

static int sample_count = 0;
int main(void)
{
	uint32_t i;
	irq_initialize_vectors();
	cpu_irq_enable();

	// Initialize the sleep manager
	sleepmgr_init();

	sysclk_init();
	board_init();
	
	/* Set default ADC test mode. */
	memset((void *)&g_adc_test_mode, 0, sizeof(g_adc_test_mode));
	g_adc_test_mode.uc_trigger_mode = TRIGGER_MODE_PWM;
	g_adc_test_mode.uc_pdc_en = 0;
	g_adc_test_mode.uc_sequence_en = 0;
	g_adc_test_mode.uc_gain_en = 1;
	g_adc_test_mode.uc_offset_en = 1;

	usb_initializer_init();
	start_adc();
	
	while(1)
	{
		if (g_adc_test_mode.uc_trigger_mode == TRIGGER_MODE_SOFTWARE) {
			mdelay(1000);
			adc_start(ADC);
		}
		
		/* Check if ADC sample is done. */
		if (g_adc_sample_data.us_done == ADC_DONE_MASK) {
			custom_printf("Sample %d", sample_count);
			sample_count++;
			for (i = 0; i < NUM_CHANNELS; i++) {

				custom_printf("CH%02d: %04d mv.    ",
				(int)g_adc_sample_data.uc_ch_num[i],
				(int)(g_adc_sample_data.
				us_value[i] *
				VOLT_REF /
				MAX_DIGITAL));				

			}
			println("\r");
			g_adc_sample_data.us_done = 0;
		}
	}
}

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

/**
 * \brief Configure to trigger ADC by TIOA output of timer.
 */
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
	tc_find_mck_divisor(1, ul_sysclk, &ul_div, &ul_tc_clks, ul_sysclk);
	tc_init(TC0, 0, ul_tc_clks | TC_CMR_CPCTRG | TC_CMR_WAVE |
			TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET);
	TC0->TC_CHANNEL[0].TC_RA = (ul_sysclk / ul_div) / 2;
	TC0->TC_CHANNEL[0].TC_RC = (ul_sysclk / ul_div) / 1;

	/* Start the Timer. */
	tc_start(TC0, 0);
	/* Set TIOA0 trigger. */

	adc_configure_trigger(ADC, ADC_TRIG_TIO_CH_0, 0);

}

/**
 * \brief Configure to trigger ADC by PWM Event Line.
 */
static void configure_pwm_trigger(void)
{
	/* PWM frequency in Hz. */
#define PWM_FREQUENCY               100
	/* Maximum duty cycle value. */
#define MAX_DUTY_CYCLE              1000

	/* Enable PWMC peripheral clock. */
	pmc_enable_periph_clk(ID_PWM);

	/* Disable PWM channel 0. */
	pwm_channel_disable(PWM, PWM_CHANNEL_0);

	gpio_configure_pin(PIN_PWMC_PWMH0_TRIG, PIN_PWMC_PWMH0_TRIG_FLAG);

	/* Set clock A to run at PWM_FREQUENCY * MAX_DUTY_CYCLE (clock B is not used). */
	pwm_clock_t pwm_clock_setting = {
		.ul_clka = PWM_FREQUENCY * MAX_DUTY_CYCLE,
		.ul_clkb = 0,
		.ul_mck = sysclk_get_cpu_hz()
	};
	pwm_init(PWM, &pwm_clock_setting);

	/* Configure PWMC for channel 0 (left-aligned). */
		pwm_channel_t pwm_trigger_channel = {
			.channel = PWM_CHANNEL_0,
			.alignment = PWM_ALIGN_LEFT,
			.polarity = PWM_LOW,
		.ul_prescaler = PWM_CMR_CPRE_CLKA,
		.ul_period = MAX_DUTY_CYCLE,
		.ul_duty = MAX_DUTY_CYCLE / 2
	};
	pwm_channel_init(PWM, &pwm_trigger_channel);

	pwm_cmp_t pwm_comparison_setting = {
		.unit = PWM_CMP_UNIT_0,
		.b_enable = true,
		.ul_value = MAX_DUTY_CYCLE / 2,
		.b_pulse_on_line_0 = true
	};
	pwm_cmp_init(PWM, &pwm_comparison_setting);


	/* Enable PWM channel 0. */
	pwm_channel_enable(PWM, PWM_CHANNEL_0);
	/* Set PWM Event Line 0 trigger. */
	adc_configure_trigger(ADC, ADC_TRIG_PWM_EVENT_LINE_0, 0);
}

/**
 * \brief Interrupt handler for the ADC.
 */
void ADC_Handler(void)
{
	uint32_t i;
	uint32_t ul_temp;
	uint8_t uc_ch_num;

	/* With PDC transfer */
	if (g_adc_test_mode.uc_pdc_en) {
		if ((adc_get_status(ADC) & ADC_ISR_RXBUFF) ==
				ADC_ISR_RXBUFF) {
			g_adc_sample_data.us_done = ADC_DONE_MASK;
			adc_read_buffer(ADC, g_adc_sample_data.us_value, BUFFER_SIZE);
			/* Only keep sample value, and discard channel number. */
			for (i = 0; i < NUM_CHANNELS; i++) {
				g_adc_sample_data.us_value[i] &= ADC_LCDR_LDATA_Msk;
			}
		}
	} else {	/* Without PDC transfer */
		if ((adc_get_status(ADC) & ADC_ISR_DRDY) ==
				ADC_ISR_DRDY) {
			ul_temp = adc_get_latest_value(ADC);
			for (i = 0; i < NUM_CHANNELS; i++) {
				uc_ch_num = (ul_temp & ADC_LCDR_CHNB_Msk) >>
						ADC_LCDR_CHNB_Pos;
				if (g_adc_sample_data.uc_ch_num[i] == uc_ch_num) {
					g_adc_sample_data.us_value[i] =
							ul_temp &
							ADC_LCDR_LDATA_Msk;
					g_adc_sample_data.us_done |= 1 << i;
				}
			}
		}
	}
}

void start_adc(void)
{
	pmc_enable_periph_clk(ID_ADC);
	adc_init(ADC, sysclk_get_cpu_hz(), 6400000, ADC_STARTUP_TIME_4);
	memset((void *)&g_adc_sample_data, 0, sizeof(g_adc_sample_data));
	adc_configure_timing(ADC, TRACKING_TIME, ADC_SETTLING_TIME_3, TRANSFER_PERIOD);
	adc_enable_tag(ADC);
	adc_configure_sequence(ADC, ch_list, 2);
	
	/* Enable channels. */
	uint32_t i;
	for (i = 0; i < NUM_CHANNELS; i++) {
		adc_enable_channel(ADC, (enum adc_channel_num_t)i);
	}
	
	/* Update channel number. */
	g_adc_sample_data.uc_ch_num[0] = ch_list[0];
	g_adc_sample_data.uc_ch_num[1] = ch_list[1];
	g_adc_sample_data.uc_ch_num[2] = ch_list[2];
	g_adc_sample_data.uc_ch_num[3] = ch_list[3];
	g_adc_sample_data.uc_ch_num[4] = ch_list[4];
	g_adc_sample_data.uc_ch_num[5] = ch_list[5];
	g_adc_sample_data.uc_ch_num[6] = ch_list[6];
	g_adc_sample_data.uc_ch_num[7] = ch_list[7];
	
	if (g_adc_test_mode.uc_offset_en)
	{
		adc_enable_anch(ADC);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_0);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_1);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_2);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_3);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_4);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_5);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_6);
		adc_enable_channel_input_offset(ADC, ADC_CHANNEL_7);
	}
	else
	{
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_0);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_1);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_2);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_3);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_4);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_5);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_6);
		adc_disable_channel_input_offset(ADC, ADC_CHANNEL_7);
	}
	
	/* Set power save. */
	if (g_adc_test_mode.uc_power_save_en) {
		adc_configure_power_save(ADC, 1, 0);
		} else {
		adc_configure_power_save(ADC, 0, 0);;
	}
	
	/* Transfer with/without PDC. */
	if (g_adc_test_mode.uc_pdc_en) {
		adc_read_buffer(ADC, g_adc_sample_data.us_value, BUFFER_SIZE);
		/* Enable PDC channel interrupt. */
		adc_enable_interrupt(ADC, ADC_IER_RXBUFF);
		} else {
		/* Enable Data ready interrupt. */
		adc_enable_interrupt(ADC, ADC_IER_DRDY);

	}
	/* Enable ADC interrupt. */
	NVIC_EnableIRQ(ADC_IRQn);
	
	/* Configure trigger mode and start convention. */
	switch (g_adc_test_mode.uc_trigger_mode) {
		case TRIGGER_MODE_SOFTWARE:
			adc_configure_trigger(ADC, ADC_TRIG_SW, 0);	/* Disable hardware trigger. */
		break;

		case TRIGGER_MODE_ADTRG:
			gpio_configure_pin(PINS_ADC_TRIG, PINS_ADC_TRIG_FLAG);
			adc_configure_trigger(ADC, ADC_TRIG_EXT, 0);
		break;

		case TRIGGER_MODE_TIMER:
			configure_time_trigger();
		break;
		case TRIGGER_MODE_PWM:
			configure_pwm_trigger();
		break;
		case TRIGGER_MODE_FREERUN:
			adc_configure_trigger(ADC, ADC_TRIG_SW, 1);
		break;
		default:
		break;
	}
}