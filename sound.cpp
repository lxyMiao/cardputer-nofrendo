/*
 * Buzzer part start rewrite from: https://github.com/moononournation/esp_8_bit.git
 */
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <driver/i2s.h>
#include <soc/ledc_struct.h>
#include <esp32-hal-timer.h>
extern "C"{
#include <nes/nes.h>
}
#include <M5Cardputer.h>
#include "hw_config.h"

#if defined(HW_AUDIO)

#define DEFAULT_FRAGSIZE 64
static void (*audio_callback)(void *buffer, int length) = NULL;
QueueHandle_t queue;
static int16_t *audio_frame;

extern "C" int osd_init_sound()
{
	audio_frame = (int16_t*) NOFRENDO_MALLOC(2 * DEFAULT_FRAGSIZE);
	audio_callback = NULL;

	return 0;
}

extern "C" void osd_stopsound()
{
	audio_callback = NULL;
}

extern "C" void do_audio_frame()
{
	int left = HW_AUDIO_SAMPLERATE / NES_REFRESH_RATE;
	while (left)
	{
		int n = DEFAULT_FRAGSIZE;
		if (n > left)
			n = left;
		audio_callback(audio_frame, n); //get more data

		//16 bit mono -> 32-bit (16 bit r+l)
		//int16_t *mono_ptr = audio_frame + n;
		//int16_t *stereo_ptr = audio_frame + n + n;
		

		//size_t i2s_bytes_write;
		//i2s_write(I2S_NUM_0, (const char *)audio_frame, 4 * n, &i2s_bytes_write, portMAX_DELAY);
		M5Cardputer.Speaker.playRaw(audio_frame,n,HW_AUDIO_SAMPLERATE);
		left -= n;
	}
	//M5Cardputer.Speaker.playRaw(audio_frame,n,HW_AUDIO_SAMPLERATE);

}

extern "C" void osd_setsound(void (*playfunc)(void *buffer, int length))
{
	//Indicates we should call playfunc() to get more data.
	audio_callback = playfunc;
}

extern "C" void osd_getsoundinfo(sndinfo_t *info)
{
	info->sample_rate = HW_AUDIO_SAMPLERATE;
	info->bps = 16;
}

#elif defined(HW_AUDIO_BUZZER)

#define DEFAULT_FRAGSIZE (HW_AUDIO_SAMPLERATE / NES_REFRESH_RATE)
static void (*audio_callback)(void *buffer, int length) = NULL;
static int16_t *audio_frame;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t audio_frame_idx = 0;

void IRAM_ATTR audioSampleTimer()
{
	audio_frame_idx++;
	if (audio_frame_idx >= DEFAULT_FRAGSIZE)
	{
		audio_frame_idx = 0;
	}

	uint16_t s = audio_frame[audio_frame_idx];
	LEDC.channel_group[0].channel[2].duty.duty = s >> 3;
	LEDC.channel_group[0].channel[2].conf0.sig_out_en = 1; // This is the output enable control bit for channel
	LEDC.channel_group[0].channel[2].conf1.duty_start = 1; // When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware
	LEDC.channel_group[0].channel[2].conf0.clk_en = 1;
}

int osd_init_sound()
{
	audio_frame = NOFRENDO_MALLOC(4 * DEFAULT_FRAGSIZE);

	ledcSetup(2, 2000000, 10);
	ledcAttachPin(HW_AUDIO_BUZZER_PIN, 2);
	ledcWrite(2, 0);

	// Use 1st timer of 4 (counted from zero).
	// Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
	// info).
	timer = timerBegin(0, 80, true);

	// Attach audioSampleTimer function to our timer.
	timerAttachInterrupt(timer, &audioSampleTimer, true);

	// Set alarm to call audioSampleTimer function every second (value in microseconds).
	// Repeat the alarm (third parameter)
	timerAlarmWrite(timer, 1000000 / HW_AUDIO_SAMPLERATE, true);

	// Start an alarm
	timerAlarmEnable(timer);

	return 0;
}

void osd_stopsound()
{
	audio_callback = NULL;
}

void do_audio_frame()
{
	audio_callback(audio_frame, DEFAULT_FRAGSIZE); //get more data
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
	//Indicates we should call playfunc() to get more data.
	audio_callback = playfunc;
}

void osd_getsoundinfo(sndinfo_t *info)
{
	info->sample_rate = HW_AUDIO_SAMPLERATE;
	info->bps = 16;
}

#else /* !defined(HW_AUDIO) */
extern "C"{
int osd_init_sound()
{
	return 0;
}

void osd_stopsound()
{
}

void do_audio_frame()
{
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
}

void osd_getsoundinfo(sndinfo_t *info)
{
	// dummy value
	info->sample_rate = 22050;
	info->bps = 16;
}
}
#endif /* !defined(HW_AUDIO) */
