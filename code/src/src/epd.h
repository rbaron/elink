#pragma once

#define epd_height 200
#define epd_width 200
#define epd_buffer_size ((epd_height/8) * epd_width)

void set_EPD_model(uint8_t model_nr);
void init_epd(void);
uint8_t EPD_read_temp(void);
void EPD_Display(unsigned char *image, int size, uint8_t full_or_partial);
void epd_display_tiff(uint8_t *pData, int iSize);
void epd_display(uint32_t time_is, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial);
void epd_set_sleep(void);
uint8_t epd_state_handler(void);
void epd_display_char(uint8_t data);
void epd_clear(void);

void epd_invert_buff(uint8_t *buff, unsigned int size);

typedef enum {
	SIXCAPS_120,
	ROBOTO_MONO_100,
	DIALOG_16,
	SPECIAL_ELITE_30,
	OPEN_SANS_8,
	OPEN_SANS_BOLD_8,
	OPEN_SANS_14,
	OPEN_SANS_BOLD_14,
	DSEG14_90,
	PRESS_START_50,
} EpdCharFont;

void epd_draw_char(char c, EpdCharFont font, uint8_t invert);

typedef struct {
	int16_t baseline_y;
	int16_t baseline_x;
	EpdCharFont font;
	char *str;
} epd_string_draw_specs_t;

void epd_draw_string(const epd_string_draw_specs_t *specs);