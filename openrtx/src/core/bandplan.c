#include "core/bandplan.h"

#include <stddef.h>

static const freq_t BANDPLAN_2M_LO = 144000000U;
static const freq_t BANDPLAN_2M_HI = 148000000U;
static const freq_t BANDPLAN_70CM_CANADA_LO = 430000000U;
static const freq_t BANDPLAN_70CM_US_LO = 420000000U;
static const freq_t BANDPLAN_70CM_HI = 450000000U;

bool bandplanIsValid(uint8_t bandplan)
{
    return bandplan < BANDPLAN_MAX;
}

const char *bandplanGetName(bandplan_t bandplan)
{
    switch(bandplan)
    {
        case BANDPLAN_OFF:
            return "Off";
        case BANDPLAN_US:
            return "US";
        case BANDPLAN_CANADA:
        default:
            return "Canada";
    }
}

bool bandplanIsFrequencyInHardwareRange(const hwInfo_t *hwinfo, freq_t freq)
{
    if(hwinfo == NULL)
        return false;

    if(hwinfo->vhf_band)
    {
        const freq_t lo = (freq_t) hwinfo->vhf_minFreq * 1000000U;
        const freq_t hi = (freq_t) hwinfo->vhf_maxFreq * 1000000U;
        if((freq >= lo) && (freq <= hi))
            return true;
    }

    if(hwinfo->uhf_band)
    {
        const freq_t lo = (freq_t) hwinfo->uhf_minFreq * 1000000U;
        const freq_t hi = (freq_t) hwinfo->uhf_maxFreq * 1000000U;
        if((freq >= lo) && (freq <= hi))
            return true;
    }

    return false;
}

bool bandplanIsFrequencyAllowed(const hwInfo_t *hwinfo, bandplan_t bandplan, freq_t freq)
{
    if(!bandplanIsFrequencyInHardwareRange(hwinfo, freq))
        return false;

    if(bandplan == BANDPLAN_OFF)
        return true;

    if((freq >= BANDPLAN_2M_LO) && (freq <= BANDPLAN_2M_HI))
        return true;

    if(bandplan == BANDPLAN_US)
        return (freq >= BANDPLAN_70CM_US_LO) && (freq <= BANDPLAN_70CM_HI);

    return (freq >= BANDPLAN_70CM_CANADA_LO) && (freq <= BANDPLAN_70CM_HI);
}
