#include "core/satellite.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "core/power.h"

#define SAT_PI 3.14159265358979323846
#define SAT_TWOPI (2.0 * SAT_PI)
#define SAT_DEG2RAD (SAT_PI / 180.0)
#define SAT_RAD2DEG (180.0 / SAT_PI)
#define SAT_EARTH_RADIUS_KM 6378.137
#define SAT_EARTH_MU_KM3_S2 398600.4418
#define SAT_LIGHT_SPEED_M_S 299792458.0

typedef struct
{
    double x;
    double y;
    double z;
}
sat_vec3_t;

static const satellite_t satellites[] =
{
    {
        "SO-50",
        436795000U,
        145850000U,
        26,
        79.61067117,
        64.5527,
        42.9946,
        0.0075384,
        278.6581,
        80.5990,
        14.82826109,
    },
    {
        "AO-27",
        436795000U,
        145850000U,
        26,
        79.47549491,
        98.6960,
        146.8926,
        0.0008393,
        8.7623,
        351.3704,
        14.30916387,
    },
    {
        "ISS FM",
        437800000U,
        145990000U,
        26,
        79.87218434,
        51.6346,
        17.0785,
        0.0006366,
        213.2716,
        146.7873,
        15.48402839,
    },
    {
        "PO-101",
        145900000U,
        437500000U,
        26,
        79.79581855,
        98.1029,
        272.2884,
        0.0012836,
        93.3044,
        266.9647,
        15.00012295,
    },
};

static bool satelliteTimeIsValid(const datetime_t *utc_time)
{
    if(utc_time == NULL)
        return false;

    return (utc_time->month >= 1) && (utc_time->month <= 12)
        && (utc_time->date >= 1) && (utc_time->date <= 31)
        && (utc_time->hour >= 0) && (utc_time->hour <= 23)
        && (utc_time->minute >= 0) && (utc_time->minute <= 59)
        && (utc_time->second >= 0) && (utc_time->second <= 59);
}

static bool satelliteObserverIsReady(const gps_t *gps)
{
    if(gps == NULL)
        return false;

    return (gps->fix_quality != FIX_QUALITY_NO_FIX)
        && (gps->fix_quality != FIX_QUALITY_ESTIMATED)
        && (gps->fix_type == FIX_TYPE_3D);
}

static double satelliteWrapRadians(double value)
{
    while(value < 0.0)
        value += SAT_TWOPI;
    while(value >= SAT_TWOPI)
        value -= SAT_TWOPI;

    return value;
}

static double satelliteJulianDate(const datetime_t *utc_time)
{
    int year = 2000 + utc_time->year;
    int month = utc_time->month;
    const double day = utc_time->date
                     + (utc_time->hour / 24.0)
                     + (utc_time->minute / 1440.0)
                     + (utc_time->second / 86400.0);

    if(month <= 2)
    {
        year -= 1;
        month += 12;
    }

    const int a = year / 100;
    const int b = 2 - a + (a / 4);

    return floor(365.25 * (year + 4716))
         + floor(30.6001 * (month + 1))
         + day + b - 1524.5;
}

static double satelliteEpochJulianDate(const satellite_t *satellite)
{
    const int full_year = (satellite->epoch_year >= 57U)
                        ? (1900 + satellite->epoch_year)
                        : (2000 + satellite->epoch_year);
    const double jan0 = floor(365.25 * (full_year - 1))
                      - floor((full_year - 1) / 100.0)
                      + floor((full_year - 1) / 400.0)
                      + 1721424.5;

    return jan0 + satellite->epoch_day;
}

static sat_vec3_t satelliteRotateZ(sat_vec3_t vec, double angle)
{
    const double c = cos(angle);
    const double s = sin(angle);
    sat_vec3_t out =
    {
        (vec.x * c) - (vec.y * s),
        (vec.x * s) + (vec.y * c),
        vec.z,
    };
    return out;
}

static sat_vec3_t satelliteEciPosition(const satellite_t *satellite,
                                       const datetime_t *utc_time)
{
    const double epoch_jd = satelliteEpochJulianDate(satellite);
    const double now_jd = satelliteJulianDate(utc_time);
    const double delta_seconds = (now_jd - epoch_jd) * 86400.0;
    const double mean_motion_rad_s = satellite->mean_motion_rev_day * SAT_TWOPI / 86400.0;
    const double semi_major_axis_km = cbrt(SAT_EARTH_MU_KM3_S2
                                           / (mean_motion_rad_s * mean_motion_rad_s));
    const double eccentricity = satellite->eccentricity;
    const double inclination = satellite->inclination_deg * SAT_DEG2RAD;
    const double raan = satellite->raan_deg * SAT_DEG2RAD;
    const double arg_perigee = satellite->arg_perigee_deg * SAT_DEG2RAD;
    const double mean_anomaly = satelliteWrapRadians((satellite->mean_anomaly_deg * SAT_DEG2RAD)
                                                     + (mean_motion_rad_s * delta_seconds));
    double eccentric_anomaly = mean_anomaly;

    for(uint8_t i = 0; i < 8; i++)
        eccentric_anomaly = mean_anomaly + (eccentricity * sin(eccentric_anomaly));

    const double cos_e = cos(eccentric_anomaly);
    const double sin_e = sin(eccentric_anomaly);
    const double radius_km = semi_major_axis_km * (1.0 - (eccentricity * cos_e));
    const double true_anomaly = atan2(sqrt(1.0 - (eccentricity * eccentricity)) * sin_e,
                                      cos_e - eccentricity);
    const double arg_lat = arg_perigee + true_anomaly;
    const double cos_arg_lat = cos(arg_lat);
    const double sin_arg_lat = sin(arg_lat);
    const double cos_raan = cos(raan);
    const double sin_raan = sin(raan);
    const double cos_inc = cos(inclination);
    const double sin_inc = sin(inclination);
    sat_vec3_t position =
    {
        radius_km * ((cos_raan * cos_arg_lat) - (sin_raan * sin_arg_lat * cos_inc)),
        radius_km * ((sin_raan * cos_arg_lat) + (cos_raan * sin_arg_lat * cos_inc)),
        radius_km * (sin_arg_lat * sin_inc),
    };

    return position;
}

static double satelliteGreenwichSidereal(const datetime_t *utc_time)
{
    const double jd = satelliteJulianDate(utc_time);
    const double t = (jd - 2451545.0) / 36525.0;
    double theta = 280.46061837
                 + (360.98564736629 * (jd - 2451545.0))
                 + (0.000387933 * t * t)
                 - ((t * t * t) / 38710000.0);

    theta = fmod(theta, 360.0);
    if(theta < 0.0)
        theta += 360.0;

    return theta * SAT_DEG2RAD;
}

static sat_vec3_t satelliteObserverEcef(const gps_t *gps)
{
    const double lat = ((double) gps->latitude / 1000000.0) * SAT_DEG2RAD;
    const double lon = ((double) gps->longitude / 1000000.0) * SAT_DEG2RAD;
    const double alt_km = ((double) gps->altitude) / 1000.0;
    const double cos_lat = cos(lat);
    const double sin_lat = sin(lat);
    const double cos_lon = cos(lon);
    const double sin_lon = sin(lon);
    const double radius = SAT_EARTH_RADIUS_KM + alt_km;
    sat_vec3_t observer =
    {
        radius * cos_lat * cos_lon,
        radius * cos_lat * sin_lon,
        radius * sin_lat,
    };
    return observer;
}

static double satelliteElevationAtTime(const satellite_t *satellite,
                                       const gps_t *gps,
                                       const datetime_t *utc_time,
                                       sat_vec3_t *range_ecef,
                                       double *range_km)
{
    const sat_vec3_t position_eci = satelliteEciPosition(satellite, utc_time);
    const sat_vec3_t position_ecef = satelliteRotateZ(position_eci,
                                                      -satelliteGreenwichSidereal(utc_time));
    const sat_vec3_t observer = satelliteObserverEcef(gps);
    sat_vec3_t range =
    {
        position_ecef.x - observer.x,
        position_ecef.y - observer.y,
        position_ecef.z - observer.z,
    };
    const double lat = ((double) gps->latitude / 1000000.0) * SAT_DEG2RAD;
    const double lon = ((double) gps->longitude / 1000000.0) * SAT_DEG2RAD;
    const double sin_lat = sin(lat);
    const double cos_lat = cos(lat);
    const double sin_lon = sin(lon);
    const double cos_lon = cos(lon);
    const double east = (-sin_lon * range.x) + (cos_lon * range.y);
    const double north = (-sin_lat * cos_lon * range.x)
                       - (sin_lat * sin_lon * range.y)
                       + (cos_lat * range.z);
    const double up = (cos_lat * cos_lon * range.x)
                    + (cos_lat * sin_lon * range.y)
                    + (sin_lat * range.z);
    const double distance = sqrt((east * east) + (north * north) + (up * up));

    if(range_ecef != NULL)
        *range_ecef = range;
    if(range_km != NULL)
        *range_km = distance;

    if(distance <= 0.0)
        return -90.0;

    return asin(up / distance) * SAT_RAD2DEG;
}

static bool satelliteFindNextEvent(const satellite_t *satellite,
                                   const gps_t *gps,
                                   const datetime_t *utc_time,
                                   bool pass_active,
                                   bool *event_is_aos,
                                   int32_t *event_seconds)
{
    datetime_t start = *utc_time;
    double previous_el = satelliteElevationAtTime(satellite, gps, &start, NULL, NULL);
    const bool search_for_rise = !pass_active;

    for(int32_t step_seconds = 60; step_seconds <= (12 * 3600); step_seconds += 60)
    {
        datetime_t probe = *utc_time;
        probe.second += step_seconds;
        realignTimeInfo(&probe);

        const double current_el = satelliteElevationAtTime(satellite, gps, &probe, NULL, NULL);
        const bool crossed_rise = (previous_el < 0.0) && (current_el >= 0.0);
        const bool crossed_set = (previous_el >= 0.0) && (current_el < 0.0);

        if((search_for_rise && crossed_rise) || (!search_for_rise && crossed_set))
        {
            int32_t low = step_seconds - 60;
            int32_t high = step_seconds;

            for(uint8_t i = 0; i < 12; i++)
            {
                const int32_t mid = (low + high) / 2;
                datetime_t mid_time = *utc_time;
                mid_time.second += mid;
                realignTimeInfo(&mid_time);

                const double mid_el = satelliteElevationAtTime(satellite, gps, &mid_time, NULL, NULL);
                if(search_for_rise)
                {
                    if(mid_el >= 0.0)
                        high = mid;
                    else
                        low = mid;
                }
                else
                {
                    if(mid_el < 0.0)
                        high = mid;
                    else
                        low = mid;
                }
            }

            if(event_is_aos != NULL)
                *event_is_aos = search_for_rise;
            if(event_seconds != NULL)
                *event_seconds = high;
            return true;
        }

        previous_el = current_el;
        start = probe;
    }

    return false;
}

size_t satelliteGetCount(void)
{
    return sizeof(satellites) / sizeof(satellites[0]);
}

const satellite_t *satelliteGetByIndex(size_t index)
{
    if(index >= satelliteGetCount())
        return NULL;

    return &satellites[index];
}

void satelliteConfigureChannel(channel_t *channel, size_t index)
{
    const satellite_t *satellite = satelliteGetByIndex(index);

    if((channel == NULL) || (satellite == NULL))
        return;

    memset(channel, 0, sizeof(*channel));
    channel->mode = OPMODE_FM;
    channel->bandwidth = BW_25;
    channel->power = powerGetDefaultStoredValue();
    channel->rx_frequency = satellite->rx_base_hz;
    channel->tx_frequency = satellite->tx_base_hz;
    strncpy(channel->name, satellite->name, sizeof(channel->name));
    strncpy(channel->descr, "Satellite", sizeof(channel->descr));
}

bool satelliteComputePrediction(size_t index, const gps_t *gps,
                                const datetime_t *utc_time,
                                satellite_prediction_t *prediction,
                                bool include_next_event)
{
    const satellite_t *satellite = satelliteGetByIndex(index);
    sat_vec3_t range_now = {0.0, 0.0, 0.0};
    double range_km = 0.0;
    datetime_t later;
    double later_range_km = 0.0;
    double elevation_deg = 0.0;
    double later_el = 0.0;

    if(prediction == NULL)
        return false;

    memset(prediction, 0, sizeof(*prediction));
    prediction->observer_ready = satelliteObserverIsReady(gps);
    prediction->time_ready = satelliteTimeIsValid(utc_time);

    if((satellite == NULL) || !prediction->observer_ready || !prediction->time_ready)
        return false;

    elevation_deg = satelliteElevationAtTime(satellite, gps, utc_time, &range_now, &range_km);
    later = *utc_time;
    later.second += 1;
    realignTimeInfo(&later);
    later_el = satelliteElevationAtTime(satellite, gps, &later, NULL, &later_range_km);

    const double lat = ((double) gps->latitude / 1000000.0) * SAT_DEG2RAD;
    const double lon = ((double) gps->longitude / 1000000.0) * SAT_DEG2RAD;
    const double sin_lat = sin(lat);
    const double cos_lat = cos(lat);
    const double sin_lon = sin(lon);
    const double cos_lon = cos(lon);
    const double east = (-sin_lon * range_now.x) + (cos_lon * range_now.y);
    const double north = (-sin_lat * cos_lon * range_now.x)
                       - (sin_lat * sin_lon * range_now.y)
                       + (cos_lat * range_now.z);
    double azimuth_deg = atan2(east, north) * SAT_RAD2DEG;
    const double range_rate_m_s = (later_range_km - range_km) * 1000.0;
    const double rx_shift = -range_rate_m_s * ((double) satellite->rx_base_hz) / SAT_LIGHT_SPEED_M_S;
    const double tx_shift = range_rate_m_s * ((double) satellite->tx_base_hz) / SAT_LIGHT_SPEED_M_S;

    if(azimuth_deg < 0.0)
        azimuth_deg += 360.0;

    prediction->geometry_valid = true;
    prediction->pass_active = elevation_deg >= 0.0;
    prediction->azimuth_deg = (int16_t) lround(azimuth_deg);
    prediction->elevation_deg = (int16_t) lround(elevation_deg);
    prediction->range_km = (uint16_t) lround(range_km);
    prediction->range_rate_m_s = (int16_t) lround(range_rate_m_s);
    prediction->rx_doppler_hz = (int32_t) lround(rx_shift / 1000.0) * 1000;
    prediction->tx_doppler_hz = (int32_t) lround(tx_shift / 1000.0) * 1000;

    if(include_next_event)
    {
        prediction->next_event_valid = satelliteFindNextEvent(satellite,
                                                              gps,
                                                              utc_time,
                                                              prediction->pass_active,
                                                              &prediction->next_event_is_aos,
                                                              &prediction->next_event_seconds);
    }

    (void) later_el;
    return true;
}
