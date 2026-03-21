#ifndef SATELLITE_H
#define SATELLITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/cps.h"
#include "core/datetime.h"
#include "core/gps.h"

typedef struct
{
    const char *name;
    freq_t rx_base_hz;
    freq_t tx_base_hz;
    uint8_t epoch_year;
    double epoch_day;
    double inclination_deg;
    double raan_deg;
    double eccentricity;
    double arg_perigee_deg;
    double mean_anomaly_deg;
    double mean_motion_rev_day;
}
satellite_t;

typedef struct
{
    bool observer_ready;
    bool time_ready;
    bool geometry_valid;
    bool pass_active;
    bool next_event_valid;
    bool next_event_is_aos;
    int16_t azimuth_deg;
    int16_t elevation_deg;
    uint16_t range_km;
    int16_t range_rate_m_s;
    int32_t rx_doppler_hz;
    int32_t tx_doppler_hz;
    int32_t next_event_seconds;
}
satellite_prediction_t;

size_t satelliteGetCount(void);
const satellite_t *satelliteGetByIndex(size_t index);
void satelliteConfigureChannel(channel_t *channel, size_t index);
bool satelliteComputePrediction(size_t index, const gps_t *gps,
                                const datetime_t *utc_time,
                                satellite_prediction_t *prediction,
                                bool include_next_event);

#endif
