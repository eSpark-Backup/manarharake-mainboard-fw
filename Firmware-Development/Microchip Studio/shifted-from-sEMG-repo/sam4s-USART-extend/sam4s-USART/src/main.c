/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include "string.h"
#include "lsm6dsr_reg.h"
#include "math.h"

#include "lsm6dsr.h"
#include "adc_serivce.h"
#include "usart_service.h"

#include "fir_filters/fir_filters.h"
#include "fir_filters/filter_info.h"

#define JSON_PACKET_TEMPLATE "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":%d,\"Y\":%d,\"Z\":%d},\"Acc\":{\"X\":%d,\"Y\":%d,\"Z\":%d}}\r\n"
#define JSON_PACKET_TEMPLATE_NO_IMU "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":0,\"Y\":0,\"Z\":0},\"Acc\":{\"X\":0,\"Y\":0,\"Z\":0}}\r\n"

#define JSON_PACKET_TEMPLATE_NO_IMU_FLOAT "{\"ADC\":{\"Value_1\":%.2f,\"Value_2\":%.2f,\"Value_3\":%.2f,\"Value_4\":%.2f,\"Value_5\":%.2f,\"Value_6\":%.2f,\"Value_7\":%.2f,\"Value_8\":%.2f},\"Gyro\":{\"X\":0,\"Y\":0,\"Z\":0},\"Acc\":{\"X\":0,\"Y\":0,\"Z\":0}}\r\n"

#define ARDUINO_PACKER_TEMPLATE "%d, %d, %d, %d, %d, %d, %d, %d"
#define ARDUINO_PACKET_TEMPLATE_FLOAT "%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f"
#define WINDOW_SIZE 100  // Fixed window size for averaging
#define BATCH_SIZE 50
#define USE_BANDPASS_FILTER 1
static float sampling_rate = 1000.0f;      // Example sampling rate
static float highpass_cutoff = 30.0f;      // High-pass cutoff frequency
static float lowpass_cutoff = 450.0f;       // Low-pass cutoff frequency

//void console_usart_irq_handler(void)
//{
	/* Get UART status and check if PDC receive buffer is full */
	//if ((usart_get_status(CONF_UART) & US_CSR_TXBUFE) ==
	//US_CSR_TXBUFE) {
		/* Configure PDC for data transfer (RX and TX) */
		//pdc_tx_init(g_p_uart_pdc, &g_pdc_uart_packet, NULL);
	//}
//}

typedef struct{
	uint16_t us_value[NUM_CHANNELS];
}adc_values_s;

typedef struct {
	float mvc_value;                    //Maximum Voluntary Contraction (MVC)

    int raw_value;                      // Store the raw ADC reading
    float normalized_value;             // Normalized value (between 0 and 1, or based on max ADC range)
    float mean_value;                   // Mean value (used for centering)
    float centered_value;               // Centered value
	
    float filtered_value;               // Final filtered value
	
	HighPassFilter hpf;                 // High-pass filter instance
	IIRLowPassFilter lpf;               // Low-pass filter instance

	FIRFilter bpf;
}adc_ctrl_t;

typedef struct {
    adc_ctrl_t adc_channels[NUM_CHANNELS];  // Array to hold ADC readings
} adc_batch_t;

typedef struct {
    float values[WINDOW_SIZE];  // Static buffer for values
    int size;                   // Current size of the buffer
    int front;                  // Index of the front element
    int rear;                   // Index of the rear element
    float sum;                  // Current sum of the values in the buffer
} sliding_moving_avg_t;


/* UART Data buffer */
char buf[256];

Pdc* g_p_uart_pdc = NULL;
static bool calibration_done = false;
pdc_packet_t g_pdc_uart_packet = {0};
static sliding_moving_avg_t emg_sliding_avg_buffers[NUM_CHANNELS] = {0};  // Buffer for each channel

// static adc_ctrl_t adc_channels[NUM_CHANNELS] = {
//     {0},    
//     {0},  
//     {0},  
//     {0},  
//     {0},  
//     {0},  
//     {0},  
//     {0},  
// };


double generate_sine_wave()
{
	static double response_duration = 600.0*2;
	static double amplitude = 2047.5;  // Half of the max value (4095/2)
	static double time = 0.0;          // Time variable for sine wave calculation
	static double freq = 30.0;          // Initial frequency in Hz
	const double freq_step = (1000.0 - 1.0) / response_duration; // Frequency step per second
	const double max_freq = 500.0;     // Maximum frequency in Hz
	
	// Generate the sine wave value at the current time
	double value = amplitude * sin(2 * M_PI * freq * time) + amplitude;
	
	// Normalize the value to be between 0 and 4095
	value = (value / (2 * amplitude)) * 4095.0; // Scale to the range 0 to 4095
	
	// Update time by the period of the timer
	time += 1.0 / 1;  // Increment time by 1 sample period
	
	// Gradually increase the frequency over 100 seconds
	if (time < response_duration) {
		freq = 1.0 + freq_step * time;
		if (freq > max_freq) freq = max_freq; // Clamp frequency to max_freq
	}
	
	return value;
}

// Function to compute the moving average
float sliding_compute_mean(sliding_moving_avg_t *buffer, float new_value) {
    // Add the new value to the buffer
    buffer->rear = (buffer->rear + 1) % WINDOW_SIZE;

    if (buffer->size < WINDOW_SIZE) {
        // If the buffer is not yet full
        buffer->size++;
    } else {
        // If the buffer is full, subtract the value being replaced from the sum
        buffer->sum -= buffer->values[buffer->rear];
    }

    // Update the sum with the new value
    buffer->values[buffer->rear] = new_value;
    buffer->sum += new_value;

    // Return the mean
    return buffer->sum / (float)buffer->size;
}

void center_emg_signal(adc_batch_t *batch, size_t num_channels) {

    // Iterate over each channel in the batch
    for (size_t ch = 0; ch < num_channels; ch++) {
        // moving_avg_t *buffer = &emg_avg_buffers[ch];  // Get the buffer for the channel
        sliding_moving_avg_t *buffer = &emg_sliding_avg_buffers[ch];

        int raw_value = batch->adc_channels[ch].raw_value;  // Get the raw sample
        // batch->adc_channels[ch].normalized_value = normalize_adc_value(raw_value);  // Normalize the value
        // batch->adc_channels[ch].normalized_value = (float)(raw_value);  // Normalize the value

        // Compute the mean using the moving average of the normalized value
        // float mean_value = compute_mean(buffer, batch->adc_channels[ch].normalized_value);
        // float mean_value = compute_mean(buffer, (float)raw_value);
        float mean_value = sliding_compute_mean(buffer, (float)raw_value);

        // Subtract the mean from the normalized value to center it around zero
        // batch->adc_channels[ch].centered_value = (float)(batch->adc_channels[ch].normalized_value - (float)mean_value);
        batch->adc_channels[ch].centered_value = (float)((float)raw_value - (float)mean_value);
        float centered_value_abs = fabsf(batch->adc_channels[ch].centered_value);
		/*batch->adc_channels[ch].centered_value = centered_value_abs;*/
/*		batch->adc_channels[ch].centered_value = (float)mean_value;*/
        if(!calibration_done)
        {
            // adc_ctrl_t *channel = &batch->adc_channels[ch];
            if((float)centered_value_abs > fabsf(batch->adc_channels[ch].mvc_value) )
            {
                batch->adc_channels[ch].mvc_value = (float)centered_value_abs;
            }
            // printf("%.2f,", batch->adc_channels[ch].mvc_value);
        }
        // batch->adc_channels[ch].centered_value = batch->adc_channels[ch].normalized_value;
        // batch->adc_channels[ch].centered_value = (float)raw_value;

        //bypassing centering for testing
        // batch->adc_channels[ch].centered_value = (float)raw_value;
    }
    if(!calibration_done)
    {
        // printf("\n");
    }
}

void bandpass_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
	for (size_t ch = 0; ch < num_channels; ch++) {
		adc_ctrl_t *channel = &batch->adc_channels[ch];
		FIRFilter *hpf = &channel->bpf; // Reference the low-pass filter for this channel

		// Apply the low-pass filter
		float input = channel->centered_value; // Input to the low-pass filter
		channel->filtered_value = FIRFilter_Update(hpf, input);
		/*channel->filtered_value = channel->centered_value;*/
		// printf("%.2f, %.2f\n", channel->filtered_value, input);
	}
}

void highpass_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
	for (size_t ch = 0; ch < num_channels; ch++) {
		adc_ctrl_t *channel = &batch->adc_channels[ch];
		HighPassFilter *hpf = &channel->hpf; // Reference the high-pass filter for this channel

		float input = channel->centered_value; // Input to the high-pass filter
		channel->filtered_value = apply_highpass_filter(hpf, input);
	}
}

void lowpass_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
	// Iterate over each channel in the batch
	for (size_t ch = 0; ch < num_channels; ch++) {
		adc_ctrl_t *channel = &batch->adc_channels[ch];
		IIRLowPassFilter *lpf = &channel->lpf; // Reference the low-pass filter for this channel

		// Apply the low-pass filter
		float input = channel->filtered_value; // Input to the low-pass filter
		channel->filtered_value = apply_lowpass_filter(lpf, input);
	}
}

void rectify_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
	// Iterate over each channel in the batch
	for (size_t ch = 0; ch < num_channels; ch++) {
		adc_ctrl_t *channel = &batch->adc_channels[ch];

		float input = channel->filtered_value; // Input to the low-pass filter
		channel->filtered_value = fabs(input);
	}
}

void normalize_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
	// Iterate over each channel in the batch
	for (size_t ch = 0; ch < num_channels; ch++) {
		adc_ctrl_t *channel = &batch->adc_channels[ch];

		float input = channel->filtered_value; // Input to the low-pass filter
		channel->filtered_value = input/channel->mvc_value;
	}
}

// void bandpass_filter_emg_signal(adc_batch_t *batch, size_t num_channels) {
//     for (size_t ch = 0; ch < num_channels; ch++) {
//         adc_ctrl_t *channel = &batch->adc_channels[ch];
//         FIRFilter *hpf = &channel->bpf; // Reference the low-pass filter for this channel

//         // Apply the low-pass filter
//         float input = channel->centered_value; // Input to the low-pass filter
//         channel->filtered_value = FIRFilter_Update(hpf, input);
//         // printf("%.2f, %.2f\n", channel->filtered_value, input);    
//     }
// }


void println(const char* str)
{
	udi_cdc_write_buf(str,strlen(str));
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void send_struct(const void* data, size_t size)
{
	udi_cdc_write_buf(data, size);
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void cdc_received()
{
	char valueBuffer;
	valueBuffer = udi_cdc_getc();
	//echo back
	udi_cdc_putc(valueBuffer);
}

adc_batch_t batch_for_dsp = {0};

int main (void)
{
	/* Insert system clock initialization code here (sysclk_init()). */
	sysclk_init();
	
	/* Initialize board */
	board_init();

	/* start USB stack */
	udc_start();
	
	/* Insert application code here */	
	imuData_t imuData;
	
	/* Initialize ADC */
	initializeAdc();
	
	/* Initialize Usart */
	initializeUart();	
		
	/* Initialize IMU */
	initializeImu();
	delay_ms(3000);
	const char *greetingMsgBuf = "Firmware Starting";
	println(greetingMsgBuf);
	
	uint32_t timeout_value = wdt_get_timeout_value(3000 * 1000, BOARD_FREQ_SLCK_XTAL);
	
	
	/* Initialize WDT with the given parameters. */
	wdt_init(WDT, WDT_MR_WDRPROC | WDT_MR_WDRSTEN | WDT_MR_WDIDLEHLT, timeout_value, timeout_value);
	
	adc_start(ADC);
	
	/*
	g_p_uart_pdc = usart_get_pdc_base(CONF_UART);	
	g_pdc_uart_packet.ul_addr = (&g_adc_sample_data + sizeof(g_adc_sample_data.uc_ch_num));
	g_pdc_uart_packet.ul_size = sizeof(g_adc_sample_data.us_value);
	pdc_tx_init(g_p_uart_pdc, &g_pdc_uart_packet, NULL);
	pdc_enable_transfer(g_p_uart_pdc, PERIPH_PTCR_TXTEN);
	usart_enable_interrupt(CONF_UART, US_IER_TXBUFE);
	NVIC_EnableIRQ(USART1_IRQn);	
	*/

	for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
		#if USE_BANDPASS_FILTER
		batch_for_dsp.adc_channels[ch].bpf.buf = (float *)malloc(FILTER_TAP_NUM * sizeof(float));
		FIRFilter_Init(&batch_for_dsp.adc_channels[ch].bpf, filter_taps, batch_for_dsp.adc_channels[ch].bpf.buf, FILTER_TAP_NUM);
		#else
		init_highpass_filter(&batch_for_dsp.adc_channels[ch].hpf, sampling_rate, highpass_cutoff);
		init_lowpass_filter(&batch_for_dsp.adc_channels[ch].lpf, sampling_rate, lowpass_cutoff);
		#endif
		batch_for_dsp.adc_channels[ch].mvc_value = 1.0f;
	}

	calibration_done = true;
	
	while(true)
	{
		// delay_ms(10);

		//getImuData(&imuData);
		adc_start(ADC);
		
		/* Check if ADC sample is done. get the samples */
		if (g_adc_sample_data.us_done == ADC_DONE_MASK) {	
								
			/* Fill the buffer to be sent via UART */
			//sprintf(buf, JSON_PACKET_TEMPLATE,
			//g_adc_sample_data.us_value[0], g_adc_sample_data.us_value[1], g_adc_sample_data.us_value[2], g_adc_sample_data.us_value[3],
			//g_adc_sample_data.us_value[4], g_adc_sample_data.us_value[5], g_adc_sample_data.us_value[6], g_adc_sample_data.us_value[7],
			// (int)imuData.gyroscope[0], (int)imuData.gyroscope[1], (int)imuData.gyroscope[2], (int)imuData.accelerometer[0], (int)imuData.accelerometer[1], (int)imuData.accelerometer[2]);
			 //g_adc_sample_data.us_done = 0;

			
// 			sprintf(buf, ARDUINO_PACKER_TEMPLATE,
// 			g_adc_sample_data.us_value[0], g_adc_sample_data.us_value[1], g_adc_sample_data.us_value[2], g_adc_sample_data.us_value[3],
// 			g_adc_sample_data.us_value[4], g_adc_sample_data.us_value[5], g_adc_sample_data.us_value[6], g_adc_sample_data.us_value[7]);
// 			g_adc_sample_data.us_done = 0;
			
			/* Send Packet to UART */
			// usartWritePacket((uint8_t*)buf);
			// buf[299] = 0;
			//println(buf);
			// memcpy(&adc_values_s, (&g_adc_sample_data + (sizeof(uint8_t) * NUM_CHANNELS)), sizeof(adc_values_s));
			// send_struct((void*)&g_adc_sample_data + sizeof(g_adc_sample_data.uc_ch_num), sizeof(g_adc_sample_data.us_value));
			// g_adc_sample_data.us_done = 0;
			
			/* construct batch */
			


			
			
			//float adc_mock = generate_sine_wave();
			batch_for_dsp.adc_channels[0].raw_value = g_adc_sample_data.us_value[0]; 			
			// batch_for_dsp.adc_channels[0].raw_value = adc_mock;
			batch_for_dsp.adc_channels[1].raw_value = g_adc_sample_data.us_value[1]; 			
			// batch_for_dsp.adc_channels[1].raw_value = adc_mock; 
			batch_for_dsp.adc_channels[2].raw_value = g_adc_sample_data.us_value[2]; 			
			// batch_for_dsp.adc_channels[2].raw_value = adc_mock; 
			batch_for_dsp.adc_channels[3].raw_value = g_adc_sample_data.us_value[3]; 			
			// batch_for_dsp.adc_channels[3].raw_value = adc_mock;
			batch_for_dsp.adc_channels[4].raw_value = g_adc_sample_data.us_value[4]; 			
			// batch_for_dsp.adc_channels[4].raw_value = adc_mock;
			batch_for_dsp.adc_channels[5].raw_value = g_adc_sample_data.us_value[5]; 			
			// batch_for_dsp.adc_channels[5].raw_value = adc_mock;
			batch_for_dsp.adc_channels[6].raw_value = g_adc_sample_data.us_value[6]; 			
			// batch_for_dsp.adc_channels[6].raw_value = adc_mock;
			batch_for_dsp.adc_channels[7].raw_value = g_adc_sample_data.us_value[7]; 			
			// batch_for_dsp.adc_channels[7].raw_value = adc_mock;

			/*Make calibration flag false after 10 seconds*/
			center_emg_signal(&batch_for_dsp, NUM_CHANNELS);
			bandpass_filter_emg_signal(&batch_for_dsp, NUM_CHANNELS);
			rectify_filter_emg_signal(&batch_for_dsp, NUM_CHANNELS);
			normalize_filter_emg_signal(&batch_for_dsp, NUM_CHANNELS);

			/*TEST THE RESULTING VALUES*/			
// 			sprintf(buf, JSON_PACKET_TEMPLATE_NO_IMU_FLOAT,
// 			batch_for_dsp.adc_channels[0].filtered_value, batch_for_dsp.adc_channels[1].filtered_value, batch_for_dsp.adc_channels[2].filtered_value, batch_for_dsp.adc_channels[3].filtered_value,
// 			batch_for_dsp.adc_channels[4].filtered_value, batch_for_dsp.adc_channels[5].filtered_value, batch_for_dsp.adc_channels[6].filtered_value, batch_for_dsp.adc_channels[7].filtered_value);
// 			g_adc_sample_data.us_done = 0;	
			
			/* Arduino serial plotter format */
			// sprintf(buf, ARDUINO_PACKET_TEMPLATE_FLOAT,
			// batch_for_dsp.adc_channels[0].filtered_value, batch_for_dsp.adc_channels[1].filtered_value, batch_for_dsp.adc_channels[2].filtered_value, batch_for_dsp.adc_channels[3].filtered_value,
			// batch_for_dsp.adc_channels[4].filtered_value, batch_for_dsp.adc_channels[5].filtered_value, batch_for_dsp.adc_channels[6].filtered_value, batch_for_dsp.adc_channels[7].filtered_value);
			// g_adc_sample_data.us_done = 0;
			
// 			sprintf(buf, ARDUINO_PACKER_TEMPLATE,
// 			batch_for_dsp.adc_channels[0].raw_value, batch_for_dsp.adc_channels[1].raw_value, batch_for_dsp.adc_channels[2].raw_value, batch_for_dsp.adc_channels[3].raw_value,
// 			batch_for_dsp.adc_channels[4].raw_value, batch_for_dsp.adc_channels[5].raw_value, batch_for_dsp.adc_channels[6].raw_value, batch_for_dsp.adc_channels[7].raw_value);
// 			g_adc_sample_data.us_done = 0;

// 			sprintf(buf, ARDUINO_PACKET_TEMPLATE_FLOAT,
// 			1.55, 2.55, 1.22, 1.22,
// 			1.22, 1.22, 1.22, 1.22);
// 			g_adc_sample_data.us_done = 0;			
// 			buf[255] = 0;
// 			println(buf);		

// 			memcpy(&adc_values_s, (&g_adc_sample_data + (sizeof(uint8_t) * NUM_CHANNELS)), sizeof(adc_values_s));
// 			send_struct((void*)&g_adc_sample_data + sizeof(g_adc_sample_data.uc_ch_num), sizeof(g_adc_sample_data.us_value));
// 			usartWritePacket((uint8_t*)buf);	
			
			//usart_serial_write_packet(CONF_UART, (void*)&g_adc_sample_data + sizeof(g_adc_sample_data.uc_ch_num), sizeof(g_adc_sample_data.us_value));
			
			char dummyPacket[64] = "Data From Atmel\r\n";
			usart_serial_write_packet((CONF_UART), dummyPacket, strlen(dummyPacket));
		}
		
		wdt_restart(WDT);
	}
}
