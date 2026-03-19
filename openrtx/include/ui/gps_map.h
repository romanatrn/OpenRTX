#ifndef UI_GPS_MAP_H
#define UI_GPS_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "core/gps.h"
#include "core/graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    GPS_MAP_ZOOM_PROVINCE = 0,
    GPS_MAP_ZOOM_REGION,
    GPS_MAP_ZOOM_TOWN,
    GPS_MAP_ZOOM_LOCAL,
    GPS_MAP_ZOOM_NUM
} gps_map_zoom_t;

void gps_map_resetTrack(void);
void gps_map_updateTrack(const gps_t *gps);
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
                  color_t accent_color);
const char *gps_map_getZoomLabel(uint8_t zoom);
uint8_t gps_map_clampZoom(int16_t zoom);
bool gps_map_hasFix(const gps_t *gps);

#ifdef __cplusplus
}
#endif

#endif
