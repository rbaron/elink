#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bw_213.h"
#include "epd_bwr_213.h"
#include "epd_bw_213_ice.h"
#include "epd_bwr_154.h"
#include "drivers.h"
#include "stack/ble/ble.h"

#include "battery.h"
#include "elink_config.h"

#include "OneBitDisplay.h"
#include "TIFF_G4.h"

// #if ELINK_CFG_ENABLE_ALL_FONTS
#include "fonts/RobotoMono_Regular100pt7b.h"
#include "fonts/SixCaps_Regular120pt7b.h"
#include "fonts/PressStart2P50pt.h"
#include "fonts/OpenSansBold14pt.h"
#include "fonts/OpenSansRegular8pt.h"
#include "fonts/OpenSansBold8pt.h"
#include "fonts/DSEG14Classic90pt.h"
// #endif  // ELINK_CFG_ENABLE_ALL_FONTS
#include "font16.h"
#include "font30.h"
#include "fonts/OpenSansRegular14pt.h"


extern const uint8_t ucMirror[];

RAM uint8_t epd_model = 0; // 0 = Undetected, 1 = BW213, 2 = BWR213, 3 = BWR154, 4 = BW213ICE
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE"};
RAM uint8_t epd_update_state = 0;

const char *BLE_conn_string[] = {"", "B"};
RAM uint8_t epd_temperature_is_read = 0;
RAM uint8_t epd_temperature = 0;

uint8_t epd_buffer[epd_buffer_size];
// uint8_t epd_buffer2[epd_buffer_size];
uint8_t epd_temp[epd_buffer_size]; // for OneBitDisplay to draw into
OBDISP obd;                        // virtual display structure
TIFFIMAGE tiff;

// With this we can force a display if it wasnt detected correctly
void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
}

// Here we detect what E-Paper display is connected
_attribute_ram_code_ void EPD_detect_model(void)
{
    EPD_init();
    // system power
    EPD_POWER_ON();

    WaitMs(10);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    // Here we neeed to detect it
    if (EPD_BWR_213_detect())
    {
        epd_model = 2;
    }
    else if (EPD_BWR_154_detect())// Right now this will never trigger, the 154 is same to 213BWR right now.
    {
        epd_model = 3;
    }
    else if (EPD_BW_213_ice_detect())
    {
        epd_model = 4;
    }
    else
    {
        epd_model = 1;
    }

    EPD_POWER_OFF();
}

_attribute_ram_code_ uint8_t EPD_read_temp(void)
{
    if (epd_temperature_is_read)
        return epd_temperature;

    if (!epd_model)
        EPD_detect_model();

    EPD_init();
    // system power
    EPD_POWER_ON();
    WaitMs(5);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_read_temp();
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_read_temp();
    else if (epd_model == 3)
        epd_temperature = EPD_BWR_154_read_temp();
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_read_temp();

    EPD_POWER_OFF();

    epd_temperature_is_read = 1;

    return epd_temperature;
}

_attribute_ram_code_ void EPD_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    if (!epd_model)
        EPD_detect_model();

    EPD_init();
    // system power
    EPD_POWER_ON();
    WaitMs(5);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_Display(image, size, full_or_partial);
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_Display(image, size, full_or_partial);
    else if (epd_model == 3)
        epd_temperature = EPD_BWR_154_Display(image, size, full_or_partial);
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_Display(image, size, full_or_partial);

    epd_temperature_is_read = 1;
    epd_update_state = 1;
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    if (!epd_model)
        EPD_detect_model();

    if (epd_model == 1)
        EPD_BW_213_set_sleep();
    else if (epd_model == 2)
        EPD_BWR_213_set_sleep();
    else if (epd_model == 3)
        EPD_BWR_154_set_sleep();
    else if (epd_model == 4)
        EPD_BW_213_ice_set_sleep();

    EPD_POWER_OFF();
    epd_update_state = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    switch (epd_update_state)
    {
    case 0:
        // Nothing todo
        break;
    case 1: // check if refresh is done and sleep epd if so
        if (epd_model == 1)
        {
            if (!EPD_IS_BUSY())
                epd_set_sleep();
        }
        else
        {
            if (EPD_IS_BUSY())
                epd_set_sleep();
        }
        break;
    }
    return epd_update_state;
}

_attribute_ram_code_ void FixBuffer(uint8_t *pSrc, uint8_t *pDst, uint16_t width, uint16_t height)
{
    int x, y;
    uint8_t *s, *d;
    for (y = 0; y < (height / 8); y++)
    { // byte rows
        d = &pDst[y];
        s = &pSrc[y * width];
        for (x = 0; x < width; x++)
        {
            d[x * (height / 8)] = ~ucMirror[s[width - 1 - x]]; // invert and flip
        }                                                      // for x
    }                                                          // for y
}

_attribute_ram_code_ void TIFFDraw(TIFFDRAW *pDraw)
{
    uint8_t uc = 0, ucSrcMask, ucDstMask, *s, *d;
    int x, y;

    s = pDraw->pPixels;
    y = pDraw->y;                          // current line
    d = &epd_buffer[(249 * 16) + (y / 8)]; // rotated 90 deg clockwise
    ucDstMask = 0x80 >> (y & 7);           // destination mask
    ucSrcMask = 0;                         // src mask
    for (x = 0; x < pDraw->iWidth; x++)
    {
        // Slower to draw this way, but it allows us to use a single buffer
        // instead of drawing and then converting the pixels to be the EPD format
        if (ucSrcMask == 0)
        { // load next source byte
            ucSrcMask = 0x80;
            uc = *s++;
        }
        if (!(uc & ucSrcMask))
        { // black pixel
            d[-(x * 16)] &= ~ucDstMask;
        }
        ucSrcMask >>= 1;
    }
}

_attribute_ram_code_ void epd_display_tiff(uint8_t *pData, int iSize)
{
    // test G4 decoder
    memset(epd_buffer, 0xff, epd_buffer_size); // clear to white
    TIFF_openRAW(&tiff, 250, 122, BITDIR_MSB_FIRST, pData, iSize, TIFFDraw);
    TIFF_setDrawParameters(&tiff, 65536, TIFF_PIXEL_1BPP, 0, 0, 250, 122, NULL);
    TIFF_decode(&tiff);
    TIFF_close(&tiff);
    EPD_Display(epd_buffer, epd_buffer_size, 1);
}

extern uint8_t mac_public[6];
_attribute_ram_code_ void epd_display(uint32_t time_is, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    if (epd_update_state)
        return;

    if (!epd_model)
    {
        EPD_detect_model();
    }
    uint16_t resolution_w = 250;
    uint16_t resolution_h = 128; // 122 real pixel, but needed to have a full byte
    if (epd_model == 1)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 2)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 3)
    {
        resolution_w = 200;
        resolution_h = 200;
    }
    else if (epd_model == 4)
    {
        resolution_w = 212;
        resolution_h = 104;
    }

    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    sprintf(buff, "ESL_%02X%02X%02X %s", mac_public[2], mac_public[1], mac_public[0], epd_model_string[epd_model]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1);
    sprintf(buff, "%s", BLE_conn_string[ble_get_connected()]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 232, 20, (char *)buff, 1);
    // sprintf(buff, "%02d:%02d", ((time_is / 60) / 60) % 24, (time_is / 60) % 60);
    // obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 50, 65, (char *)buff, 1);
    sprintf(buff, "%d'C", EPD_read_temp());
    obdWriteStringCustom(&obd, (GFXfont *)&Special_Elite_Regular_30, 10, 95, (char *)buff, 1);
    sprintf(buff, "Battery %dmV", battery_mv);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1);
    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);
    EPD_Display(epd_buffer, resolution_w * resolution_h / 8, full_or_partial);
}

static void rotate90(const uint8_t *src, uint8_t *dst, unsigned int a, unsigned int b) {
    unsigned int dst_idx = 0;

    for (int x = 0; x < b; x++) {
        for (int y = a - 1; y >= 0; y--) {
            unsigned int dst_byte = dst_idx / 8;
            unsigned int dst_bit = dst_idx % 8;

            unsigned int src_idx = y * b + x;
            unsigned int src_byte = src_idx / 8;
            unsigned int src_bit = src_idx % 8;

            dst[dst_byte] |= (((src[src_byte] >> (7-src_bit)) & 0x1) << (7-dst_bit));
            dst_idx++;
        }
    }
}

static void draw_single_char(
        uint8_t *buff,
        const GFXfont *font,
        const GFXglyph *glyph,
        int baseline_y,
        int baseline_x) {

    for (int y = 0; y < glyph->height; y++) {
        for (int x = 0; x < glyph->width; x++) {
            uint32_t byte = glyph->bitmapOffset + (y * glyph->width + x) / 8;
            uint8_t bit = (y * glyph->width + x) % 8;
            uint8_t src_val = (font->bitmap[byte] >> (7 - bit)) & 0x1;

            int dst_y = baseline_y + glyph->yOffset + y;
            int dst_x = baseline_x + glyph->xOffset + x;
            int dst_idx = dst_y * 128 + dst_x;

            if (dst_y < 250 && dst_y >= 0 && dst_x >=0 && dst_x < 128) {
                buff[dst_idx / 8] |= src_val << (7 - (dst_idx % 8));
            } else {
                // printf("[draw_single_char] Error dst_y = %d, dst_x = %d\n", dst_y, dst_x);
            }
        }
    }
}

static GFXfont* get_gfx_font(EpdCharFont font) {
    switch (font) {
#if ELINK_CFG_ENABLE_ALL_FONTS
        // case ROBOTO_MONO_100:
        //     return (GFXfont *) &RobotoMono_Regular100pt7b;
        // case SIXCAPS_120:
        //     return (GFXfont *) &SixCaps_Regular120pt7b;
        // case DIALOG_16:
        //     return (GFXfont *) &Dialog_plain_16;
        // case SPECIAL_ELITE_30:
        //     return (GFXfont *) &Special_Elite_Regular_30;
        // case OPEN_SANS_8:
        //     return (GFXfont *) &OpenSans_Regular8pt7b;
        // case OPEN_SANS_BOLD_8:
        //     return (GFXfont *) &OpenSans_Bold8pt7b;
        // case OPEN_SANS_14:
        //     return (GFXfont *) &OpenSans_Regular14pt7b;
        // case OPEN_SANS_BOLD_14:
        //     return (GFXfont *) &OpenSans_Bold14pt7b;
        // case DSEG14_90:
        //     return (GFXfont *) &DSEG14Classic_Regular90pt7b;
        case PRESS_START_50:
            return (GFXfont *) &PressStart2P_Regular50pt7b;
#endif  // ELINK_CFG_ENABLE_ALL_FONTS
        default:
            return (GFXfont *) &OpenSans_Regular14pt7b;
    }
}

void epd_invert_buff(uint8_t *buff, unsigned int size) {
    while (size--) {
        buff[size] = ~buff[size];
    }
}

void epd_draw_char(char c, EpdCharFont font, uint8_t invert) {
    const GFXfont *gfx_font = get_gfx_font(font);
    const GFXglyph *glyph = &gfx_font->glyph[c - 0x20];

    int baseline_y = 230;
    int baseline_x = (128 - glyph->width) / 2 - glyph->xOffset;

    memset(epd_buffer, 0x00, 250 * 128 / 8);
    draw_single_char(epd_buffer, gfx_font, glyph, baseline_y, baseline_x);

    // Image is currently inverted. If invert is set, we do nothing.
    if (!invert) {
        epd_invert_buff(epd_buffer, 250 * 128 / 8);
    }

    EPD_Display(epd_buffer, 250 * 128 / 8, /*full_or_partial=*/true);
}

void epd_draw_string(const epd_string_draw_specs_t *specs) {
    const GFXfont *gfx_font = get_gfx_font(specs->font);
    GFXglyph *glyph;
    int base_x = specs->baseline_x;
    char *str = specs->str;

    do {
        glyph = &gfx_font->glyph[*str - 0x20];
        draw_single_char(epd_buffer, gfx_font, glyph, specs->baseline_y, base_x);
        base_x += glyph->xAdvance;
    } while (*++str);
}

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    int i;
    for (i = 0; i < epd_buffer_size; i++)
    {
        epd_buffer[i] = data;
    }
    EPD_Display(epd_buffer, epd_buffer_size, 1);
}

_attribute_ram_code_ void epd_clear(void)
{
    memset(epd_buffer, 0x00, epd_buffer_size);
}
