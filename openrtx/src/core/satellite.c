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

static double satelliteElevationAtTime(const satellite_t *satellite,
                                       const gps_t *gps,
                                       const datetime_t *utc_time,
                                       sat_vec3_t *range_ecef,
                                       double *range_km);
static bool satelliteFindNextEvent(const satellite_t *satellite,
                                   const gps_t *gps,
                                   const datetime_t *utc_time,
                                   bool pass_active,
                                   bool *event_is_aos,
                                   int32_t *event_seconds);

static const satellite_t satellites[] =
{
    {
        "AO-91",
        145960000U,
        435250000U,
        26,
        80.58148605,
        97.4831,
        309.6288,
        0.0153435,
        270.8313,
        87.5340,
        15.11111116,
    },
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
        "CAS-3H",
        437200000U,
        144350000U,
        26,
        80.60094370,
        97.4785,
        114.2410,
        0.0006912,
        195.8764,
        164.2270,
        15.48449986,
    },
    {
        "IO-86",
        435880000U,
        145880000U,
        26,
        51.50840545,
        6.0006,
        282.0515,
        0.0012733,
        338.8432,
        194.8877,
        14.79136711,
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
    {
        "RS95S",
        436950000U,
        145920000U,
        26,
        80.49200436,
        97.3985,
        156.7512,
        0.0007960,
        306.1923,
        53.8578,
        15.23834312,
    },
    {
        "SO-125",
        436666000U,
        145875000U,
        26,
        80.51048045,
        97.3925,
        341.4183,
        0.0008973,
        88.9270,
        271.3015,
        15.60341731,
    },
    {
        "AO-123",
        435400000U,
        145850000U,
        26,
        80.61714332,
        97.3128,
        310.0935,
        0.0012394,
        261.8461,
        98.1377,
        15.34286716,
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
        && (gps->fix_type != FIX_TYPE_NOT_AVAIL);
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

static void satelliteSubPointFromEcef(sat_vec3_t position_ecef,
                                      int32_t *latitude_e6,
                                      int32_t *longitude_e6)
{
    const double radius = sqrt((position_ecef.x * position_ecef.x)
                             + (position_ecef.y * position_ecef.y)
                             + (position_ecef.z * position_ecef.z));
    double longitude = atan2(position_ecef.y, position_ecef.x) * SAT_RAD2DEG;
    const double latitude = asin(position_ecef.z / radius) * SAT_RAD2DEG;

    if(longitude > 180.0)
        longitude -= 360.0;
    else if(longitude < -180.0)
        longitude += 360.0;

    if(latitude_e6 != NULL)
        *latitude_e6 = (int32_t) lround(latitude * 1000000.0);
    if(longitude_e6 != NULL)
        *longitude_e6 = (int32_t) lround(longitude * 1000000.0);
}

static bool satelliteComputePredictionAtTime(const satellite_t *satellite,
                                             const gps_t *gps,
                                             const datetime_t *utc_time,
                                             satellite_prediction_t *prediction,
                                             bool include_next_event,
                                             int32_t *sub_latitude,
                                             int32_t *sub_longitude)
{
    sat_vec3_t range_now = {0.0, 0.0, 0.0};
    const sat_vec3_t position_eci = satelliteEciPosition(satellite, utc_time);
    const sat_vec3_t position_ecef = satelliteRotateZ(position_eci,
                                                      -satelliteGreenwichSidereal(utc_time));
    double range_km = 0.0;
    datetime_t later;
    double later_range_km = 0.0;
    double elevation_deg = 0.0;

    if(prediction == NULL)
        return false;

    memset(prediction, 0, sizeof(*prediction));
    prediction->observer_ready = satelliteObserverIsReady(gps);
    prediction->time_ready = satelliteTimeIsValid(utc_time);

    if(!prediction->time_ready)
        return false;

    satelliteSubPointFromEcef(position_ecef, sub_latitude, sub_longitude);

    if(!prediction->observer_ready)
        return false;

    elevation_deg = satelliteElevationAtTime(satellite, gps, utc_time, &range_now, &range_km);
    later = *utc_time;
    later.second += 1;
    realignTimeInfo(&later);
    satelliteElevationAtTime(satellite, gps, &later, NULL, &later_range_km);

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

    return true;
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

    if((satellite == NULL) || (prediction == NULL))
        return false;

    return satelliteComputePredictionAtTime(satellite, gps, utc_time,
                                            prediction, include_next_event,
                                            NULL, NULL);
}

bool satelliteComputeSubPoint(size_t index, const datetime_t *utc_time,
                              int32_t *latitude_e6, int32_t *longitude_e6)
{
    const satellite_t *satellite = satelliteGetByIndex(index);
    sat_vec3_t position_eci;
    sat_vec3_t position_ecef;

    if((satellite == NULL) || !satelliteTimeIsValid(utc_time))
        return false;

    position_eci = satelliteEciPosition(satellite, utc_time);
    position_ecef = satelliteRotateZ(position_eci,
                                     -satelliteGreenwichSidereal(utc_time));

    satelliteSubPointFromEcef(position_ecef, latitude_e6, longitude_e6);
    return true;
}

size_t satelliteSampleTrack(size_t index, const gps_t *gps,
                            const datetime_t *utc_time,
                            int32_t start_offset_seconds,
                            int32_t step_seconds,
                            size_t max_points,
                            satellite_track_point_t *points)
{
    const satellite_t *satellite = satelliteGetByIndex(index);
    size_t count = 0;

    if((satellite == NULL) || (gps == NULL) || (utc_time == NULL) ||
       (points == NULL) || (max_points == 0U) || (step_seconds <= 0))
        return 0U;

    for(size_t i = 0; i < max_points; i++)
    {
        datetime_t sample_time = *utc_time;
        satellite_prediction_t prediction;
        int32_t sub_latitude = 0;
        int32_t sub_longitude = 0;
        const int32_t offset_seconds = start_offset_seconds + (step_seconds * (int32_t) i);

        memset(&points[i], 0, sizeof(points[i]));
        sample_time.second += offset_seconds;
        realignTimeInfo(&sample_time);

        if(!satelliteComputePredictionAtTime(satellite, gps, &sample_time,
                                             &prediction, false,
                                             &sub_latitude, &sub_longitude))
            continue;

        points[i].valid = prediction.time_ready;
        points[i].offset_seconds = offset_seconds;
        points[i].azimuth_deg = prediction.azimuth_deg;
        points[i].elevation_deg = prediction.elevation_deg;
        points[i].sub_latitude = sub_latitude;
        points[i].sub_longitude = sub_longitude;
        count++;
    }

    return count;
}
