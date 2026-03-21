#ifndef BANDPLAN_H
#define BANDPLAN_H

#include <stdbool.h>
#include <stdint.h>
#include "core/datatypes.h"
#include "interfaces/platform.h"

typedef enum
{
    BANDPLAN_CANADA = 0,
    BANDPLAN_US,
    BANDPLAN_OFF,
    BANDPLAN_MAX
}
bandplan_t;

bool bandplanIsValid(uint8_t bandplan);
const char *bandplanGetName(bandplan_t bandplan);
bool bandplanIsFrequencyInHardwareRange(const hwInfo_t *hwinfo, freq_t freq);
bool bandplanIsFrequencyAllowed(const hwInfo_t *hwinfo, bandplan_t bandplan, freq_t freq);

#endif
