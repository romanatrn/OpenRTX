#include "core/repeater.h"

#include <limits.h>
#include <string.h>

#include "interfaces/cps_io.h"

#define REPEATER_COORD_SCALE 100
#define REPEATER_METERS_PER_DEGREE 111320

static repeater_nearest_entry_t nearest_cache[REPEATER_NEAREST_MAX];
static uint16_t nearest_cache_count;
static bool nearest_cache_valid;
static int32_t nearest_cache_lat;
static int32_t nearest_cache_lon;

static const uint16_t cos_q15_lut[] = {
    32768, 31650, 28378, 23170, 16384, 8481, 0
};

static uint32_t isqrt64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t) 1 << 62;

    while(bit > value)
        bit >>= 2;

    while(bit != 0)
    {
        if(value >= (result + bit))
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }

        bit >>= 2;
    }

    return (uint32_t) result;
}

static uint16_t interpolateCosQ15(uint32_t latitude_e6)
{
    uint32_t abs_lat = latitude_e6;
    uint32_t segment;
    uint32_t remainder;

    if(abs_lat > 90000000u)
        abs_lat = 90000000u;

    segment = abs_lat / 15000000u;
    if(segment >= 6u)
        return cos_q15_lut[6];

    remainder = abs_lat % 15000000u;

    return (uint16_t) (cos_q15_lut[segment]
                     - ((uint32_t) (cos_q15_lut[segment] - cos_q15_lut[segment + 1])
                      * remainder) / 15000000u);
}

static uint32_t abs32(int32_t value)
{
    return (value < 0) ? (uint32_t) (-value) : (uint32_t) value;
}

static int32_t geoComponentToE6(int32_t integer_part, uint16_t decimal_part)
{
    int32_t value = integer_part * 1000000;
    int32_t decimal = (int32_t) decimal_part * REPEATER_COORD_SCALE;

    if(integer_part < 0)
        return value - decimal;

    return value + decimal;
}

static void e6ToGeoComponent(int32_t value_e6, int32_t *integer_part,
                             uint16_t *decimal_part)
{
    int32_t integer = value_e6 / 1000000;
    int32_t decimal = value_e6 % 1000000;

    if(decimal < 0)
        decimal = -decimal;

    if(integer_part != NULL)
        *integer_part = integer;
    if(decimal_part != NULL)
        *decimal_part = (uint16_t) (decimal / REPEATER_COORD_SCALE);
}

static bool shouldRefreshNearestCache(const gps_t *gps)
{
    if(!nearest_cache_valid)
        return true;
    if(!repeater_hasGpsFix(gps))
        return true;

    return (abs32(gps->latitude - nearest_cache_lat) > 1000u)
        || (abs32(gps->longitude - nearest_cache_lon) > 1000u);
}

bool repeater_hasGpsFix(const gps_t *gps)
{
    if(gps == NULL)
        return false;

    return (gps->fix_quality != FIX_QUALITY_NO_FIX)
        && (gps->fix_quality != FIX_QUALITY_ESTIMATED)
        && (gps->fix_type != FIX_TYPE_NOT_AVAIL);
}

bool repeater_getChannelLocation(const channel_t *channel,
                                 int32_t *latitude_e6,
                                 int32_t *longitude_e6)
{
    int32_t lat;
    int32_t lon;

    if(channel == NULL)
        return false;
    if(channel->ch_location.ch_lat_dec >= 10000u)
        return false;
    if(channel->ch_location.ch_lon_dec >= 10000u)
        return false;

    lat = geoComponentToE6(channel->ch_location.ch_lat_int,
                           channel->ch_location.ch_lat_dec);
    lon = geoComponentToE6(channel->ch_location.ch_lon_int,
                           channel->ch_location.ch_lon_dec);

    if((lat == 0) && (lon == 0))
        return false;
    if((lat < -90000000) || (lat > 90000000))
        return false;
    if((lon < -180000000) || (lon > 180000000))
        return false;

    if(latitude_e6 != NULL)
        *latitude_e6 = lat;
    if(longitude_e6 != NULL)
        *longitude_e6 = lon;

    return true;
}

bool repeater_hasValidLocation(const channel_t *channel)
{
    return repeater_getChannelLocation(channel, NULL, NULL);
}

bool repeater_setChannelLocationFromGps(channel_t *channel, const gps_t *gps)
{
    int32_t lat_integer;
    int32_t lon_integer;
    uint16_t lat_decimal;
    uint16_t lon_decimal;

    if((channel == NULL) || !repeater_hasGpsFix(gps))
        return false;

    e6ToGeoComponent(gps->latitude, &lat_integer, &lat_decimal);
    e6ToGeoComponent(gps->longitude, &lon_integer, &lon_decimal);

    if((lat_integer < INT8_MIN) || (lat_integer > INT8_MAX))
        return false;
    if((lon_integer < INT16_MIN) || (lon_integer > INT16_MAX))
        return false;

    channel->ch_location.ch_lat_int = (int8_t) lat_integer;
    channel->ch_location.ch_lat_dec = lat_decimal;
    channel->ch_location.ch_lon_int = (int16_t) lon_integer;
    channel->ch_location.ch_lon_dec = lon_decimal;
    channel->ch_location.ch_altitude = 0;

    return true;
}

void repeater_clearChannelLocation(channel_t *channel)
{
    if(channel == NULL)
        return;

    memset(&channel->ch_location, 0x00, sizeof(channel->ch_location));
}

uint32_t repeater_distanceMeters(const gps_t *gps, const channel_t *channel)
{
    int32_t lat_e6;
    int32_t lon_e6;
    uint32_t dlat_e6;
    uint32_t dlon_e6;
    uint32_t avg_lat_e6;
    uint64_t dy_m;
    uint64_t dx_m;
    uint16_t cos_q15;

    if(!repeater_hasGpsFix(gps))
        return UINT32_MAX;
    if(!repeater_getChannelLocation(channel, &lat_e6, &lon_e6))
        return UINT32_MAX;

    dlat_e6 = abs32(lat_e6 - gps->latitude);
    dlon_e6 = abs32(lon_e6 - gps->longitude);
    avg_lat_e6 = (abs32(lat_e6) + abs32(gps->latitude)) / 2u;
    cos_q15 = interpolateCosQ15(avg_lat_e6);

    dy_m = ((uint64_t) dlat_e6 * REPEATER_METERS_PER_DEGREE) / 1000000u;
    dx_m = ((uint64_t) dlon_e6 * REPEATER_METERS_PER_DEGREE * cos_q15)
         / (1000000u * 32768u);

    return isqrt64((dx_m * dx_m) + (dy_m * dy_m));
}

uint16_t repeater_getChannelCount(void)
{
    channel_t channel;
    uint16_t index = 0;

    while((index < UINT16_MAX) && (cps_readChannel(&channel, index + 1u) == 0))
        index++;

    return index;
}

void repeater_invalidateNearestCache(void)
{
    nearest_cache_valid = false;
    nearest_cache_count = 0;
    nearest_cache_lat = 0;
    nearest_cache_lon = 0;
    memset(nearest_cache, 0x00, sizeof(nearest_cache));
}

static void refreshNearestCache(const gps_t *gps)
{
    const uint16_t channel_count = repeater_getChannelCount();

    repeater_invalidateNearestCache();

    if(!repeater_hasGpsFix(gps))
        return;

    nearest_cache_lat = gps->latitude;
    nearest_cache_lon = gps->longitude;

    for(uint16_t logical_index = 0; logical_index < channel_count; logical_index++)
    {
        channel_t channel;
        uint32_t distance_m;
        uint16_t insert_at = nearest_cache_count;

        if(cps_readChannel(&channel, logical_index + 1u) != 0)
            continue;
        if(!repeater_hasValidLocation(&channel))
            continue;

        distance_m = repeater_distanceMeters(gps, &channel);
        if(distance_m == UINT32_MAX)
            continue;

        if((nearest_cache_count == REPEATER_NEAREST_MAX)
        && (distance_m >= nearest_cache[REPEATER_NEAREST_MAX - 1u].distance_m))
            continue;

        if(insert_at > REPEATER_NEAREST_MAX - 1u)
            insert_at = REPEATER_NEAREST_MAX - 1u;

        while((insert_at > 0u) && (nearest_cache[insert_at - 1u].distance_m > distance_m))
        {
            if(insert_at < REPEATER_NEAREST_MAX)
                nearest_cache[insert_at] = nearest_cache[insert_at - 1u];
            insert_at--;
        }

        nearest_cache[insert_at].channel_index = logical_index;
        nearest_cache[insert_at].distance_m = distance_m;

        if(nearest_cache_count < REPEATER_NEAREST_MAX)
            nearest_cache_count++;
    }

    nearest_cache_valid = true;
}

uint16_t repeater_getNearestCount(const gps_t *gps)
{
    if(shouldRefreshNearestCache(gps))
        refreshNearestCache(gps);

    return nearest_cache_count;
}

int32_t repeater_getNearestChannelIndex(const gps_t *gps, uint16_t pos)
{
    if(pos >= repeater_getNearestCount(gps))
        return -1;

    return nearest_cache[pos].channel_index;
}

uint32_t repeater_getNearestDistance(const gps_t *gps, uint16_t pos)
{
    if(pos >= repeater_getNearestCount(gps))
        return UINT32_MAX;

    return nearest_cache[pos].distance_m;
}

bool repeater_isNearestChannel(const gps_t *gps, uint16_t channel_index)
{
    const uint16_t count = repeater_getNearestCount(gps);

    for(uint16_t i = 0; i < count; i++)
    {
        if(nearest_cache[i].channel_index == channel_index)
            return true;
    }

    return false;
}
