/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui/gps_map.h"

#include <stdint.h>
#include <string.h>

#include "core/utils.h"
#include "hwconfig.h"

#define GPS_MAP_NS_CENTER_LAT  45100000
#define GPS_MAP_NS_CENTER_LON -62850000
#define GPS_MAP_TRACK_POINTS       24
#define GPS_MAP_MIN_TRACK_STEP   1500
#define GPS_MAP_MAX_POLY_POINTS  640

typedef struct
{
    int32_t lat;
    int32_t lon;
} gps_map_point_t;

typedef enum
{
    GPS_MAP_FEATURE_COAST = 0,
    GPS_MAP_FEATURE_WATER,
    GPS_MAP_FEATURE_MAJOR_ROAD,
    GPS_MAP_FEATURE_SECONDARY_ROAD
} gps_map_feature_kind_t;

typedef struct
{
    const gps_map_point_t *points;
    uint16_t count;
    int32_t min_lat;
    int32_t max_lat;
    int32_t min_lon;
    int32_t max_lon;
    uint8_t min_zoom;
    uint8_t kind;
} gps_map_feature_t;

typedef struct
{
    int32_t lat_span;
    int32_t lon_span;
    const char *label;
} gps_map_zoom_info_t;

typedef struct
{
    const char *name;
    int32_t lat;
    int32_t lon;
    uint8_t min_zoom;
} gps_map_label_t;

typedef struct
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} gps_map_viewport_t;

static const gps_map_zoom_info_t zoom_levels[GPS_MAP_ZOOM_NUM] = {
    {5200000, 8600000, "160km"},
    {1800000, 3000000, "60km"},
    { 520000,  760000, "20km"},
    { 180000,  280000, "5km"},
    {  36000,   56000, "1km"}
};

#include "gps_map_data.inc"

static int32_t track_lat[GPS_MAP_TRACK_POINTS];
static int32_t track_lon[GPS_MAP_TRACK_POINTS];
static uint8_t track_head;
static uint8_t track_len;
static int32_t last_track_lat;
static int32_t last_track_lon;
static point_t gps_map_projected[GPS_MAP_MAX_POLY_POINTS];
static int16_t gps_map_nodes[GPS_MAP_MAX_POLY_POINTS];

static inline int32_t abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static bool gps_map_pointInViewport(point_t point, const gps_map_viewport_t *viewport)
{
    return (point.x >= viewport->x0) && (point.x <= viewport->x1) &&
           (point.y >= viewport->y0) && (point.y <= viewport->y1);
}

static uint8_t gps_map_outCode(point_t point, const gps_map_viewport_t *viewport)
{
    uint8_t code = 0U;

    if(point.x < viewport->x0) code |= 1U;
    if(point.x > viewport->x1) code |= 2U;
    if(point.y < viewport->y0) code |= 4U;
    if(point.y > viewport->y1) code |= 8U;

    return code;
}

static bool gps_map_clipLine(point_t *start, point_t *end,
                             const gps_map_viewport_t *viewport)
{
    uint8_t start_code = gps_map_outCode(*start, viewport);
    uint8_t end_code = gps_map_outCode(*end, viewport);

    while(true)
    {
        int32_t x;
        int32_t y;
        uint8_t out_code;

        if((start_code | end_code) == 0U)
            return true;

        if((start_code & end_code) != 0U)
            return false;

        out_code = (start_code != 0U) ? start_code : end_code;

        if((out_code & 8U) != 0U)
        {
            x = start->x + ((int32_t)(end->x - start->x) * (viewport->y1 - start->y)) /
                           (end->y - start->y);
            y = viewport->y1;
        }
        else if((out_code & 4U) != 0U)
        {
            x = start->x + ((int32_t)(end->x - start->x) * (viewport->y0 - start->y)) /
                           (end->y - start->y);
            y = viewport->y0;
        }
        else if((out_code & 2U) != 0U)
        {
            y = start->y + ((int32_t)(end->y - start->y) * (viewport->x1 - start->x)) /
                           (end->x - start->x);
            x = viewport->x1;
        }
        else
        {
            y = start->y + ((int32_t)(end->y - start->y) * (viewport->x0 - start->x)) /
                           (end->x - start->x);
            x = viewport->x0;
        }

        if(out_code == start_code)
        {
            start->x = (int16_t)x;
            start->y = (int16_t)y;
            start_code = gps_map_outCode(*start, viewport);
        }
        else
        {
            end->x = (int16_t)x;
            end->y = (int16_t)y;
            end_code = gps_map_outCode(*end, viewport);
        }
    }
}

static void gps_map_drawClippedLine(point_t start, point_t end,
                                    const gps_map_viewport_t *viewport,
                                    color_t color)
{
    if(gps_map_clipLine(&start, &end, viewport))
        gfx_drawLine(start, end, color);
}

static bool gps_map_intersects(const gps_map_feature_t *feature,
                               int32_t min_lat,
                               int32_t max_lat,
                               int32_t min_lon,
                               int32_t max_lon)
{
    if(feature->max_lat < min_lat) return false;
    if(feature->min_lat > max_lat) return false;
    if(feature->max_lon < min_lon) return false;
    if(feature->min_lon > max_lon) return false;

    return true;
}

static point_t gps_map_project(point_t origin,
                               uint16_t width,
                               uint16_t height,
                               int32_t center_lat,
                               int32_t center_lon,
                               int32_t lat_span,
                               int32_t lon_span,
                               gps_map_point_t point)
{
    int64_t sx = (int64_t)(point.lon - center_lon + (lon_span / 2));
    int64_t sy = (int64_t)(center_lat + (lat_span / 2) - point.lat);
    point_t screen = {
        origin.x + (int16_t)((sx * (width - 1U)) / lon_span),
        origin.y + (int16_t)((sy * (height - 1U)) / lat_span)
    };

    return screen;
}

static color_t gps_map_featureColor(uint8_t kind,
                                    color_t base_color,
                                    color_t road_color,
                                    color_t accent_color)
{
#ifdef CONFIG_PIX_FMT_RGB565
    static const color_t water_color = { 80, 150, 220, 255 };
    static const color_t coast_color = { 255, 255, 255, 255 };
#else
    static const color_t water_color = { 160, 160, 160, 255 };
    static const color_t coast_color = { 255, 255, 255, 255 };
#endif

    switch(kind)
    {
        case GPS_MAP_FEATURE_COAST:          return coast_color;
        case GPS_MAP_FEATURE_WATER:          return water_color;
        case GPS_MAP_FEATURE_MAJOR_ROAD:     return accent_color;
        case GPS_MAP_FEATURE_SECONDARY_ROAD: return road_color;
        default:                             return base_color;
    }
}

static color_t gps_map_backgroundColor(void)
{
#ifdef CONFIG_PIX_FMT_RGB565
    static const color_t background = { 12, 28, 48, 255 };
#else
    static const color_t background = { 255, 255, 255, 255 };
#endif

    return background;
}

static color_t gps_map_labelColor(void)
{
#ifdef CONFIG_PIX_FMT_RGB565
    static const color_t label = { 255, 230, 160, 255 };
#else
    static const color_t label = { 0, 0, 0, 255 };
#endif

    return label;
}

static color_t gps_map_landColor(void)
{
#ifdef CONFIG_PIX_FMT_RGB565
    static const color_t land = { 36, 72, 38, 255 };
#else
    static const color_t land = { 200, 200, 200, 255 };
#endif

    return land;
}

static void gps_map_fillPolygon(point_t origin,
                                uint16_t width,
                                uint16_t height,
                                int32_t center_lat,
                                int32_t center_lon,
                                int32_t lat_span,
                                int32_t lon_span,
                                const gps_map_point_t *points,
                                uint16_t count,
                                color_t color)
{
    int16_t y;
    uint16_t i;

    if((count < 3U) || (count > GPS_MAP_MAX_POLY_POINTS))
        return;

    for(i = 0U; i < count; i++)
        gps_map_projected[i] = gps_map_project(origin, width, height, center_lat,
                                               center_lon, lat_span, lon_span,
                                               points[i]);

    for(y = origin.y; y < (origin.y + (int16_t)height); y++)
    {
        uint16_t node_count = 0U;
        uint16_t j = count - 1U;
        uint16_t k;

        for(i = 0U; i < count; i++)
        {
            int16_t yi = gps_map_projected[i].y;
            int16_t yj = gps_map_projected[j].y;

            if(((yi < y) && (yj >= y)) || ((yj < y) && (yi >= y)))
            {
                int32_t dy = (int32_t)yj - yi;
                if(dy != 0)
                {
                    int32_t x = gps_map_projected[i].x +
                                ((int32_t)(y - yi) * ((int32_t)gps_map_projected[j].x - gps_map_projected[i].x)) / dy;
                    if(node_count < GPS_MAP_MAX_POLY_POINTS)
                        gps_map_nodes[node_count++] = (int16_t)x;
                }
            }

            j = i;
        }

        for(i = 1U; i < node_count; i++)
        {
            int16_t value = gps_map_nodes[i];
            k = i;

            while((k > 0U) && (gps_map_nodes[k - 1U] > value))
            {
                gps_map_nodes[k] = gps_map_nodes[k - 1U];
                k--;
            }

            gps_map_nodes[k] = value;
        }

        for(i = 0U; (i + 1U) < node_count; i += 2U)
        {
            int16_t x0 = gps_map_nodes[i];
            int16_t x1 = gps_map_nodes[i + 1U];

            if(x0 < origin.x)
                x0 = origin.x;
            if(x1 >= (origin.x + (int16_t)width))
                x1 = origin.x + (int16_t)width - 1;

            while(x0 <= x1)
            {
                gfx_setPixel((point_t){x0, y}, color);
                x0++;
            }
        }
    }
}

static void gps_map_drawLand(point_t origin,
                             uint16_t width,
                             uint16_t height,
                             int32_t center_lat,
                             int32_t center_lon,
                             int32_t lat_span,
                             int32_t lon_span)
{
    uint8_t i;
    color_t land_color = gps_map_landColor();

    for(i = 0U; i < ARRAY_SIZE(map_land_polygons); i++)
    {
        gps_map_fillPolygon(origin, width, height, center_lat, center_lon,
                            lat_span, lon_span,
                            map_land_polygons[i].points,
                            map_land_polygons[i].count,
                            land_color);
    }
}

static void gps_map_drawPolyline(point_t origin,
                                 uint16_t width,
                                 uint16_t height,
                                 const gps_map_viewport_t *viewport,
                                 int32_t center_lat,
                                 int32_t center_lon,
                                 int32_t lat_span,
                                 int32_t lon_span,
                                 const gps_map_feature_t *feature,
                                 color_t color)
{
    uint16_t i;

    for(i = 1U; i < feature->count; i++)
    {
        point_t start = gps_map_project(origin, width, height, center_lat,
                                        center_lon, lat_span, lon_span,
                                        feature->points[i - 1U]);
        point_t end = gps_map_project(origin, width, height, center_lat,
                                      center_lon, lat_span, lon_span,
                                      feature->points[i]);
        gps_map_drawClippedLine(start, end, viewport, color);

        if(feature->kind == GPS_MAP_FEATURE_COAST)
        {
            gps_map_drawClippedLine((point_t){start.x, (int16_t)(start.y + 1)},
                                    (point_t){end.x, (int16_t)(end.y + 1)},
                                    viewport, color);
        }
    }
}

static void gps_map_drawLabels(point_t origin,
                               uint16_t width,
                               uint16_t height,
                               const gps_map_viewport_t *viewport,
                               int32_t center_lat,
                               int32_t center_lon,
                               int32_t lat_span,
                               int32_t lon_span,
                               uint8_t zoom)
{
    uint8_t i;
    color_t label_color = gps_map_labelColor();

    for(i = 0U; i < ARRAY_SIZE(map_labels); i++)
    {
        point_t pos;

        if(map_labels[i].min_zoom > zoom)
            continue;

        pos = gps_map_project(origin, width, height, center_lat, center_lon,
                              lat_span, lon_span,
                              (gps_map_point_t){ map_labels[i].lat, map_labels[i].lon });

        if((pos.x < (viewport->x0 + 2)) || (pos.x > (viewport->x1 - 28)) ||
           (pos.y < (viewport->y0 + 5)) || (pos.y > (viewport->y1 - 6)))
            continue;

        gfx_setPixel(pos, label_color);
        gfx_print((point_t){(int16_t)(pos.x + 2), (int16_t)(pos.y - 3)}, FONT_SIZE_5PT,
                  TEXT_ALIGN_LEFT, label_color, "%s", map_labels[i].name);
    }
}

static void gps_map_drawTrack(point_t origin,
                              uint16_t width,
                              uint16_t height,
                              const gps_map_viewport_t *viewport,
                              int32_t center_lat,
                              int32_t center_lon,
                              int32_t lat_span,
                              int32_t lon_span,
                              color_t color)
{
    uint8_t i;

    if(track_len < 2U)
        return;

    for(i = 1U; i < track_len; i++)
    {
        uint8_t start_idx = (uint8_t)((track_head + GPS_MAP_TRACK_POINTS - track_len + i - 1U) % GPS_MAP_TRACK_POINTS);
        uint8_t end_idx = (uint8_t)((track_head + GPS_MAP_TRACK_POINTS - track_len + i) % GPS_MAP_TRACK_POINTS);
        gps_map_point_t start_pt = { track_lat[start_idx], track_lon[start_idx] };
        gps_map_point_t end_pt = { track_lat[end_idx], track_lon[end_idx] };
        point_t start = gps_map_project(origin, width, height, center_lat,
                                        center_lon, lat_span, lon_span, start_pt);
        point_t end = gps_map_project(origin, width, height, center_lat,
                                      center_lon, lat_span, lon_span, end_pt);
        gps_map_drawClippedLine(start, end, viewport, color);
    }
}

bool gps_map_hasFix(const gps_t *gps)
{
    if(gps == NULL)
        return false;

    return gps->fix_quality != FIX_QUALITY_NO_FIX &&
           gps->fix_quality != FIX_QUALITY_ESTIMATED;
}

void gps_map_resetTrack(void)
{
    memset(track_lat, 0x00, sizeof(track_lat));
    memset(track_lon, 0x00, sizeof(track_lon));
    track_head = 0U;
    track_len = 0U;
    last_track_lat = 0;
    last_track_lon = 0;
}

void gps_map_updateTrack(const gps_t *gps)
{
    int32_t dlat;
    int32_t dlon;

    if(!gps_map_hasFix(gps))
        return;

    if(track_len == 0U)
    {
        track_lat[0] = gps->latitude;
        track_lon[0] = gps->longitude;
        track_head = 1U;
        track_len = 1U;
        last_track_lat = gps->latitude;
        last_track_lon = gps->longitude;
        return;
    }

    dlat = abs32(gps->latitude - last_track_lat);
    dlon = abs32(gps->longitude - last_track_lon);
    if((dlat < GPS_MAP_MIN_TRACK_STEP) && (dlon < GPS_MAP_MIN_TRACK_STEP))
        return;

    track_lat[track_head] = gps->latitude;
    track_lon[track_head] = gps->longitude;
    track_head = (uint8_t)((track_head + 1U) % GPS_MAP_TRACK_POINTS);
    if(track_len < GPS_MAP_TRACK_POINTS)
        track_len++;

    last_track_lat = gps->latitude;
    last_track_lon = gps->longitude;
}

uint8_t gps_map_clampZoom(int16_t zoom)
{
    if(zoom < 0)
        return GPS_MAP_ZOOM_PROVINCE;
    if(zoom >= GPS_MAP_ZOOM_NUM)
        return GPS_MAP_ZOOM_NUM - 1U;

    return (uint8_t)zoom;
}

const char *gps_map_getZoomLabel(uint8_t zoom)
{
    return zoom_levels[gps_map_clampZoom((int16_t)zoom)].label;
}

void gps_map_draw(point_t origin,
                  uint16_t width,
                  uint16_t height,
                  const gps_t *gps,
                  uint8_t zoom,
                  bool use_manual_center,
                  int32_t manual_center_lat,
                  int32_t manual_center_lon,
                  color_t base_color,
                  color_t road_color,
                  color_t accent_color)
{
    uint8_t i;
    int32_t center_lat = GPS_MAP_NS_CENTER_LAT;
    int32_t center_lon = GPS_MAP_NS_CENTER_LON;
    const gps_map_zoom_info_t *zoom_info = &zoom_levels[gps_map_clampZoom((int16_t)zoom)];
    gps_map_viewport_t viewport = {
        origin.x + 1,
        origin.y + 1,
        origin.x + (int16_t)width - 2,
        origin.y + (int16_t)height - 2
    };
    int32_t min_lat;
    int32_t max_lat;
    int32_t min_lon;
    int32_t max_lon;

    if(use_manual_center)
    {
        center_lat = manual_center_lat;
        center_lon = manual_center_lon;
    }
    else if((zoom != GPS_MAP_ZOOM_PROVINCE) && gps_map_hasFix(gps))
    {
        center_lat = gps->latitude;
        center_lon = gps->longitude;
    }

    min_lat = center_lat - (zoom_info->lat_span / 2);
    max_lat = center_lat + (zoom_info->lat_span / 2);
    min_lon = center_lon - (zoom_info->lon_span / 2);
    max_lon = center_lon + (zoom_info->lon_span / 2);

    gfx_drawRect(origin, width, height, gps_map_backgroundColor(), true);
    gps_map_drawLand(origin, width, height, center_lat, center_lon,
                     zoom_info->lat_span, zoom_info->lon_span);
    gfx_drawRect(origin, width, height, base_color, false);

    for(i = 0U; i < ARRAY_SIZE(map_features); i++)
    {
        color_t color;

        if(map_features[i].min_zoom > zoom)
            continue;
        if(!gps_map_intersects(&map_features[i], min_lat, max_lat, min_lon, max_lon))
            continue;

        color = gps_map_featureColor(map_features[i].kind, base_color,
                                     road_color, accent_color);
        gps_map_drawPolyline(origin, width, height, &viewport,
                             center_lat, center_lon,
                             zoom_info->lat_span, zoom_info->lon_span,
                             &map_features[i], color);
    }

    gps_map_drawLabels(origin, width, height, &viewport, center_lat, center_lon,
                       zoom_info->lat_span, zoom_info->lon_span, zoom);

    gps_map_drawTrack(origin, width, height, &viewport, center_lat, center_lon,
                      zoom_info->lat_span, zoom_info->lon_span, accent_color);

    if(gps_map_hasFix(gps))
    {
        uint16_t marker_radius = (CONFIG_SCREEN_HEIGHT > 100) ? 4U : 3U;
        point_t pos = gps_map_project(origin, width, height, center_lat, center_lon,
                                      zoom_info->lat_span, zoom_info->lon_span,
                                      (gps_map_point_t){ gps->latitude, gps->longitude });
        if(gps_map_pointInViewport(pos, &viewport))
        {
            if((pos.x >= (viewport.x0 + (int16_t)marker_radius)) &&
               (pos.x <= (viewport.x1 - (int16_t)marker_radius)) &&
               (pos.y >= (viewport.y0 + (int16_t)marker_radius)) &&
               (pos.y <= (viewport.y1 - (int16_t)marker_radius)))
            {
                gfx_drawCircle(pos, marker_radius, accent_color);
            }
            else
            {
                gfx_setPixel(pos, accent_color);
            }
            gps_map_drawClippedLine((point_t){pos.x - 3, pos.y},
                                    (point_t){pos.x + 3, pos.y},
                                    &viewport, accent_color);
            gps_map_drawClippedLine((point_t){pos.x, pos.y - 3},
                                    (point_t){pos.x, pos.y + 3},
                                    &viewport, accent_color);
        }
    }

    gfx_drawRect(origin, width, height, base_color, false);
}
