#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/ui_default.h"

#define SATELLITE_VIEW_INFO 0U
#define SATELLITE_VIEW_POLAR 1U
#define SATELLITE_VIEW_TIMELINE 2U
#define SATELLITE_VIEW_GROUND 3U
#define SATELLITE_VIEW_COUNT 4U

#define SAT_POLAR_POINTS 31U
#define SAT_TIMELINE_POINTS 31U
#define SAT_GROUND_POINTS 31U

static const char *satellite_view_names[SATELLITE_VIEW_COUNT] =
{
    "Info",
    "Polar",
    "Pass",
    "Track"
};

static int16_t _ui_satClamp16(int16_t value, int16_t min_value, int16_t max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;

    return value;
}

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

static void _ui_satBodyRect(point_t *origin, uint16_t *width, uint16_t *height)
{
    origin->x = 1;
    origin->y = layout.top_h + 1;
    *width = CONFIG_SCREEN_WIDTH - 2U;
    *height = CONFIG_SCREEN_HEIGHT - layout.top_h - layout.bottom_h - 2U;
}

static void _ui_satDrawFooter(const char *left_text, const char *right_text)
{
    gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_LEFT,
              color_white, "%s", left_text);
    gfx_print(layout.bottom_pos, FONT_SIZE_5PT, TEXT_ALIGN_RIGHT,
              color_white, "%s", right_text);
}

static void _ui_satDrawTitle(const satellite_t *satellite, uint8_t view)
{
    char title[24] = {0};

    if(satellite == NULL)
    {
        gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER,
                  color_white, "Satellite");
        return;
    }

    snprintf(title, sizeof(title), "%s %s",
             satellite->name, satellite_view_names[view % SATELLITE_VIEW_COUNT]);
    gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER,
              color_white, "%s", title);
}

static bool _ui_satReadyForObserver(const satellite_prediction_t *prediction,
                                    const satellite_t *satellite)
{
    if(satellite == NULL)
        return false;

    if((prediction == NULL) || !prediction->observer_ready)
    {
        gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Need 3D GPS fix");
        gfx_print(layout.line3_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Need open sky");
        _ui_satDrawFooter("L/R tune  # auto", "0 view");
        return false;
    }

    if(!prediction->time_ready)
    {
        gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
                  color_white, "Need UTC time");
        _ui_satDrawFooter("L/R tune  # auto", "0 view");
        return false;
    }

    return true;
}

static point_t _ui_satPolarProject(point_t center, uint16_t radius,
                                   int16_t azimuth_deg, int16_t elevation_deg)
{
    const double azimuth = ((double) azimuth_deg) * (M_PI / 180.0);
    const double scaled_radius = ((90.0 - (double) elevation_deg) / 90.0) * radius;
    point_t point =
    {
        center.x + (int16_t) lround(scaled_radius * sin(azimuth)),
        center.y - (int16_t) lround(scaled_radius * cos(azimuth))
    };

    return point;
}

static void _ui_satDrawMarker(point_t point, color_t color)
{
    point_t origin = {point.x - 1, point.y - 1};
    gfx_drawRect(origin, 3, 3, color, true);
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

    _ui_satDrawFooter("ENT open", "ESC back");
}

static void _ui_drawSatelliteInfoPage(ui_state_t *ui_state,
                                      const satellite_t *satellite,
                                      const satellite_prediction_t *prediction)
{
    char buf[24] = {0};
    char rx_buf[20] = {0};
    char tx_buf[20] = {0};

    if(!_ui_satReadyForObserver(prediction, satellite))
        return;

    _ui_satFormatEvent(buf, sizeof(buf), prediction);
    gfx_print(layout.line1_pos, layout.line1_font, TEXT_ALIGN_LEFT,
              color_white, "Az %03d", prediction->azimuth_deg);
    gfx_print(layout.line1_pos, layout.line1_font, TEXT_ALIGN_RIGHT,
              color_white, "El %02d", prediction->elevation_deg);
    gfx_print(layout.line2_pos, layout.line2_font, TEXT_ALIGN_CENTER,
              color_white, "%s", buf);

    _ui_satFormatFrequency(rx_buf, sizeof(rx_buf), last_state.channel.rx_frequency);
    _ui_satFormatFrequency(tx_buf, sizeof(tx_buf), last_state.channel.tx_frequency);
    snprintf(buf, sizeof(buf), "RX %s", rx_buf);
    gfx_print(layout.line3_large_pos, layout.line3_large_font, TEXT_ALIGN_CENTER,
              color_white, "%s", buf);
    gfx_print(layout.line4_pos, layout.line4_font, TEXT_ALIGN_LEFT,
              color_white, "TX %s", tx_buf);
    gfx_print(layout.line4_pos, layout.line4_font, TEXT_ALIGN_RIGHT,
              color_white, "R %uk", (unsigned int) prediction->range_km);

    snprintf(buf, sizeof(buf), "%s %+ldk",
             ui_state->satellite_auto_doppler ? "AUTO" : "MAN",
             (long) (ui_state->satellite_manual_bias_hz / 1000));
    _ui_satDrawFooter(buf, "0 view");
}

static void _ui_drawSatellitePolarPage(const satellite_t *satellite,
                                       const satellite_prediction_t *prediction,
                                       const gps_t *gps,
                                       const datetime_t *utc_time)
{
    point_t origin;
    uint16_t width;
    uint16_t height;
    point_t center;
    uint16_t radius;
    satellite_track_point_t points[SAT_POLAR_POINTS];

    if(!_ui_satReadyForObserver(prediction, satellite))
        return;

    _ui_satBodyRect(&origin, &width, &height);
    center.x = origin.x + (int16_t) (width / 2U);
    center.y = origin.y + (int16_t) (height / 2U);
    radius = (width < height) ? (width / 2U) - 7U : (height / 2U) - 5U;

    gfx_drawCircle(center, radius, color_white);
    gfx_drawCircle(center, radius * 2U / 3U, color_grey);
    gfx_drawCircle(center, radius / 3U, color_grey);
    gfx_drawLine((point_t){center.x - (int16_t) radius, center.y},
                 (point_t){center.x + (int16_t) radius, center.y}, color_grey);
    gfx_drawLine((point_t){center.x, center.y - (int16_t) radius},
                 (point_t){center.x, center.y + (int16_t) radius}, color_grey);

    gfx_print((point_t){center.x, (int16_t)(center.y - radius - 8)}, FONT_SIZE_5PT,
              TEXT_ALIGN_CENTER, color_white, "N");
    gfx_print((point_t){(int16_t)(center.x + radius + 2), center.y}, FONT_SIZE_5PT,
              TEXT_ALIGN_LEFT, color_white, "E");
    gfx_print((point_t){center.x, (int16_t)(center.y + radius + 2)}, FONT_SIZE_5PT,
              TEXT_ALIGN_CENTER, color_white, "S");
    gfx_print((point_t){(int16_t)(center.x - radius - 6), center.y}, FONT_SIZE_5PT,
              TEXT_ALIGN_LEFT, color_white, "W");

    satelliteSampleTrack((size_t) ui_state.satellite_selected,
                         gps,
                         utc_time,
                         -900,
                         60,
                         SAT_POLAR_POINTS,
                         points);

    for(uint8_t i = 1U; i < SAT_POLAR_POINTS; i++)
    {
        if(points[i - 1U].valid && points[i].valid &&
           (points[i - 1U].elevation_deg >= 0) && (points[i].elevation_deg >= 0))
        {
            const point_t start = _ui_satPolarProject(center, radius,
                                                      points[i - 1U].azimuth_deg,
                                                      points[i - 1U].elevation_deg);
            const point_t end = _ui_satPolarProject(center, radius,
                                                    points[i].azimuth_deg,
                                                    points[i].elevation_deg);
            gfx_drawLine(start, end,
                         (points[i].offset_seconds < 0) ? color_grey : color_white);
        }
    }

    if(prediction->elevation_deg >= 0)
        _ui_satDrawMarker(_ui_satPolarProject(center, radius,
                                              prediction->azimuth_deg,
                                              prediction->elevation_deg),
                          yellow_fab413);

    _ui_satDrawFooter("Sky map", "0 view");
}

static int16_t _ui_satTimelineY(point_t origin, uint16_t height, int16_t elevation_deg)
{
    const int16_t clamped = _ui_satClamp16(elevation_deg, -10, 90);
    const int32_t scaled = ((int32_t) (clamped + 10) * (int32_t) (height - 1U)) / 100;

    return (int16_t) (origin.y + height - 1U - scaled);
}

static void _ui_drawSatelliteTimelinePage(const satellite_prediction_t *prediction,
                                          const satellite_t *satellite,
                                          const gps_t *gps,
                                          const datetime_t *utc_time)
{
    point_t origin;
    uint16_t width;
    uint16_t height;
    satellite_track_point_t points[SAT_TIMELINE_POINTS];
    char event_buf[24] = {0};

    if(!_ui_satReadyForObserver(prediction, satellite))
        return;

    _ui_satBodyRect(&origin, &width, &height);
    gfx_drawRect(origin, width, height, color_grey, false);

    origin.x += 2;
    origin.y += 2;
    width -= 4U;
    height -= 4U;

    satelliteSampleTrack((size_t) ui_state.satellite_selected,
                         gps,
                         utc_time,
                         -900,
                         120,
                         SAT_TIMELINE_POINTS,
                         points);

    gfx_drawLine((point_t){origin.x, _ui_satTimelineY(origin, height, 0)},
                 (point_t){(int16_t)(origin.x + width - 1U), _ui_satTimelineY(origin, height, 0)},
                 color_grey);

    for(uint8_t i = 1U; i < SAT_TIMELINE_POINTS; i++)
    {
        if(points[i - 1U].valid && points[i].valid)
        {
            point_t start =
            {
                origin.x + (int16_t) (((uint32_t) (i - 1U) * (width - 1U)) / (SAT_TIMELINE_POINTS - 1U)),
                _ui_satTimelineY(origin, height, points[i - 1U].elevation_deg)
            };
            point_t end =
            {
                origin.x + (int16_t) (((uint32_t) i * (width - 1U)) / (SAT_TIMELINE_POINTS - 1U)),
                _ui_satTimelineY(origin, height, points[i].elevation_deg)
            };
            gfx_drawLine(start, end, color_white);
        }
    }

    for(uint8_t i = 0U; i < SAT_TIMELINE_POINTS; i++)
    {
        if(points[i].valid && (points[i].offset_seconds == 0))
        {
            const point_t now_point =
            {
                origin.x + (int16_t) (((uint32_t) i * (width - 1U)) / (SAT_TIMELINE_POINTS - 1U)),
                _ui_satTimelineY(origin, height, points[i].elevation_deg)
            };
            _ui_satDrawMarker(now_point, yellow_fab413);
            break;
        }
    }

    gfx_print((point_t){origin.x, (int16_t)(origin.y + height - 8)}, FONT_SIZE_5PT,
              TEXT_ALIGN_LEFT, color_white, "-15m");
    gfx_print((point_t){(int16_t)(origin.x + width - 1U), (int16_t)(origin.y + height - 8)},
              FONT_SIZE_5PT, TEXT_ALIGN_RIGHT, color_white, "+45m");
    _ui_satFormatEvent(event_buf, sizeof(event_buf), prediction);
    _ui_satDrawFooter(event_buf, "0 view");
}

static point_t _ui_satGroundProject(point_t origin, uint16_t width, uint16_t height,
                                    int32_t latitude_e6, int32_t longitude_e6)
{
    const int64_t norm_lon = (int64_t) longitude_e6 + 180000000LL;
    const int64_t norm_lat = 90000000LL - (int64_t) latitude_e6;
    point_t point =
    {
        origin.x + (int16_t) ((norm_lon * (width - 1U)) / 360000000LL),
        origin.y + (int16_t) ((norm_lat * (height - 1U)) / 180000000LL)
    };

    return point;
}

static void _ui_drawSatelliteGroundPage(const satellite_prediction_t *prediction,
                                        const satellite_t *satellite,
                                        const gps_t *gps,
                                        const datetime_t *utc_time)
{
    point_t origin;
    uint16_t width;
    uint16_t height;
    satellite_track_point_t points[SAT_GROUND_POINTS];
    char buf[24] = {0};

    if(!_ui_satReadyForObserver(prediction, satellite))
        return;

    _ui_satBodyRect(&origin, &width, &height);
    gfx_drawRect(origin, width, height, color_grey, false);

    for(int16_t lon = -120; lon <= 120; lon += 60)
    {
        const point_t start = _ui_satGroundProject(origin, width, height, 90000000, lon * 1000000);
        const point_t end = _ui_satGroundProject(origin, width, height, -90000000, lon * 1000000);
        gfx_drawLine(start, end, color_grey);
    }

    for(int16_t lat = -60; lat <= 60; lat += 30)
    {
        const point_t start = _ui_satGroundProject(origin, width, height, lat * 1000000, -180000000);
        const point_t end = _ui_satGroundProject(origin, width, height, lat * 1000000, 180000000);
        gfx_drawLine(start, end, color_grey);
    }

    satelliteSampleTrack((size_t) ui_state.satellite_selected,
                         gps,
                         utc_time,
                         -1800,
                         180,
                         SAT_GROUND_POINTS,
                         points);

    for(uint8_t i = 1U; i < SAT_GROUND_POINTS; i++)
    {
        if(points[i - 1U].valid && points[i].valid)
        {
            const point_t start = _ui_satGroundProject(origin, width, height,
                                                       points[i - 1U].sub_latitude,
                                                       points[i - 1U].sub_longitude);
            const point_t end = _ui_satGroundProject(origin, width, height,
                                                     points[i].sub_latitude,
                                                     points[i].sub_longitude);
            if(abs(end.x - start.x) < (int16_t) (width / 2U))
                gfx_drawLine(start, end,
                             (points[i].offset_seconds < 0) ? color_grey : color_white);
        }
    }

    _ui_satDrawMarker(_ui_satGroundProject(origin, width, height,
                                           gps->latitude, gps->longitude),
                      color_white);

    for(uint8_t i = 0U; i < SAT_GROUND_POINTS; i++)
    {
        if(points[i].valid && (points[i].offset_seconds == 0))
        {
            _ui_satDrawMarker(_ui_satGroundProject(origin, width, height,
                                                   points[i].sub_latitude,
                                                   points[i].sub_longitude),
                              yellow_fab413);
            break;
        }
    }

    snprintf(buf, sizeof(buf), "Obs %c%.0f Sat %s",
             (gps->latitude >= 0) ? 'N' : 'S',
             fabs((double) gps->latitude / 1000000.0),
             prediction->pass_active ? "up" : "dn");
    _ui_satDrawFooter(buf, "0 view");
}

void _ui_drawSatelliteInfo(ui_state_t *ui_state)
{
    const satellite_t *satellite = satelliteGetByIndex(ui_state->satellite_selected);
    const satellite_prediction_t *prediction = &ui_state->satellite_prediction;
    datetime_t utc_time;

    _ui_clearScreen();
    _ui_satDrawTitle(satellite, ui_state->satellite_view);

#ifdef CONFIG_RTC
    utc_time = last_state.time;
#else
    utc_time = last_state.gps_data.timestamp;
#endif

    switch(ui_state->satellite_view % SATELLITE_VIEW_COUNT)
    {
        case SATELLITE_VIEW_POLAR:
            _ui_drawSatellitePolarPage(satellite, prediction, &last_state.gps_data, &utc_time);
            break;

        case SATELLITE_VIEW_TIMELINE:
            _ui_drawSatelliteTimelinePage(prediction, satellite, &last_state.gps_data, &utc_time);
            break;

        case SATELLITE_VIEW_GROUND:
            _ui_drawSatelliteGroundPage(prediction, satellite, &last_state.gps_data, &utc_time);
            break;

        case SATELLITE_VIEW_INFO:
        default:
            _ui_drawSatelliteInfoPage(ui_state, satellite, prediction);
            break;
    }
}
