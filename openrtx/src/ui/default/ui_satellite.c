#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ui/ui_default.h"

static void _ui_satFormatFrequency(char *buf, size_t max_len, freq_t frequency)
{
    snprintf(buf, max_len, "%u.%03u",
             (unsigned int) (frequency / 1000000U),
             (unsigned int) ((frequency / 1000U) % 1000U));
}

static void _ui_satFormatEvent(char *buf, size_t max_len,
                               const satellite_prediction_t *prediction)
{
    if((prediction == NULL) || !prediction->next_event_valid)
    {
        snprintf(buf, max_len, "Pass n/a");
        return;
    }

    const uint32_t total = (prediction->next_event_seconds < 0)
                         ? 0U
                         : (uint32_t) prediction->next_event_seconds;
    const uint32_t minutes = total / 60U;
    const uint32_t seconds = total % 60U;

    snprintf(buf, max_len, "%s %02u:%02u",
             prediction->next_event_is_aos ? "AOS" : "LOS",
             (unsigned int) minutes,
             (unsigned int) seconds);
}

void _ui_drawMenuSatellites(ui_state_t *ui_state)
{
    point_t pos = layout.line1_pos;
    const size_t count = satelliteGetCount();
    const uint8_t entries_in_screen = (CONFIG_SCREEN_HEIGHT - 1 - pos.y) / layout.menu_h + 1;
    uint8_t scroll = 0;

    _ui_clearScreen();
    gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER,
              color_white, "Satellites");

    if(count == 0U)
    {
        gfx_print(layout.line3_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "No satellites");
        return;
    }

    if(ui_state->menu_selected >= count)
        ui_state->menu_selected = count - 1U;

    if(ui_state->menu_selected >= entries_in_screen)
        scroll = ui_state->menu_selected - entries_in_screen + 1U;

    for(uint8_t item = 0; item < entries_in_screen; item++)
    {
        const uint8_t index = item + scroll;
        const satellite_t *satellite;
        color_t text_color = color_white;

        if(index >= count)
            break;

        satellite = satelliteGetByIndex(index);
        if(satellite == NULL)
            continue;

        if(index == ui_state->menu_selected)
        {
            point_t rect_pos = {0, pos.y - layout.menu_h + 3};
            text_color = color_black;
            gfx_drawRect(rect_pos, CONFIG_SCREEN_WIDTH, layout.menu_h, yellow_fab413, true);
        }

        gfx_print(pos, layout.menu_font, TEXT_ALIGN_LEFT, text_color, "%s", satellite->name);
        pos.y += layout.menu_h;
    }

    gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_LEFT,
              color_white, "ENT Open");
    gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_RIGHT,
              color_white, "ESC Back");
}

void _ui_drawSatelliteInfo(ui_state_t *ui_state)
{
    char buf[24] = {0};
    char rx_buf[16] = {0};
    char tx_buf[16] = {0};
    const satellite_t *satellite = satelliteGetByIndex(ui_state->satellite_selected);
    const satellite_prediction_t *prediction = &ui_state->satellite_prediction;

    _ui_clearScreen();

    gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER,
              color_white, (satellite != NULL) ? satellite->name : "Satellite");

    if((satellite == NULL) || !prediction->observer_ready)
    {
        gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Need 3D GPS fix");
        gfx_print(layout.line3_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Select outside with GPS on");
    }
    else if(!prediction->time_ready)
    {
        gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Need UTC time");
    }
    else
    {
        _ui_satFormatEvent(buf, sizeof(buf), prediction);
        gfx_print(layout.line1_pos, layout.line1_font, TEXT_ALIGN_LEFT,
                  color_white, "Az %03d", prediction->azimuth_deg);
        gfx_print(layout.line1_pos, layout.line1_font, TEXT_ALIGN_RIGHT,
                  color_white, "El %02d", prediction->elevation_deg);
        gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "%s", buf);

        _ui_satFormatFrequency(rx_buf, sizeof(rx_buf), last_state.channel.rx_frequency);
        _ui_satFormatFrequency(tx_buf, sizeof(tx_buf), last_state.channel.tx_frequency);
        gfx_print(layout.line3_large_pos, layout.line3_large_font, TEXT_ALIGN_CENTER,
                  color_white, "%s", rx_buf);
        gfx_print(layout.line4_pos, layout.line4_font, TEXT_ALIGN_CENTER,
                  color_white, "TX %s  R %ukm", tx_buf,
                  (unsigned int) prediction->range_km);

        snprintf(buf, sizeof(buf), "%s  trim %+ldk",
                 ui_state->satellite_auto_doppler ? "AUTO" : "MAN",
                 (long) (ui_state->satellite_manual_bias_hz / 1000));
        gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_LEFT,
                  color_white, "%s", buf);
    }

    gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_RIGHT,
              color_white, "# Auto  ENT 0");
}
