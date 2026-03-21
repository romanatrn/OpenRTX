#include "core/preset_channels.h"

#include <string.h>

#include "core/power.h"
#include "interfaces/platform.h"

typedef struct
{
    const char *name;
    freq_t rx;
    freq_t tx;
    uint8_t bandwidth;
    bool rx_only;
}
presetChannelDef_t;

typedef struct
{
    uint16_t bankId;
    const char *name;
    const presetChannelDef_t *channels;
    uint16_t count;
    bool tx_requires_off_bandplan;
}
presetBankDef_t;

static const presetChannelDef_t frsChannels[] =
{
    { "FRS 1",  462562500U, 462562500U, BW_12_5, false },
    { "FRS 2",  462587500U, 462587500U, BW_12_5, false },
    { "FRS 3",  462612500U, 462612500U, BW_12_5, false },
    { "FRS 4",  462637500U, 462637500U, BW_12_5, false },
    { "FRS 5",  462662500U, 462662500U, BW_12_5, false },
    { "FRS 6",  462687500U, 462687500U, BW_12_5, false },
    { "FRS 7",  462712500U, 462712500U, BW_12_5, false },
    { "FRS 8",  467562500U, 467562500U, BW_12_5, false },
    { "FRS 9",  467587500U, 467587500U, BW_12_5, false },
    { "FRS 10", 467612500U, 467612500U, BW_12_5, false },
    { "FRS 11", 467637500U, 467637500U, BW_12_5, false },
    { "FRS 12", 467662500U, 467662500U, BW_12_5, false },
    { "FRS 13", 467687500U, 467687500U, BW_12_5, false },
    { "FRS 14", 467712500U, 467712500U, BW_12_5, false },
    { "FRS 15", 462550000U, 462550000U, BW_12_5, false },
    { "FRS 16", 462575000U, 462575000U, BW_12_5, false },
    { "FRS 17", 462600000U, 462600000U, BW_12_5, false },
    { "FRS 18", 462625000U, 462625000U, BW_12_5, false },
    { "FRS 19", 462650000U, 462650000U, BW_12_5, false },
    { "FRS 20", 462675000U, 462675000U, BW_12_5, false },
    { "FRS 21", 462700000U, 462700000U, BW_12_5, false },
    { "FRS 22", 462725000U, 462725000U, BW_12_5, false },
};

static const presetChannelDef_t mursChannels[] =
{
    { "MURS 1", 151820000U, 151820000U, BW_12_5, false },
    { "MURS 2", 151880000U, 151880000U, BW_12_5, false },
    { "MURS 3", 151940000U, 151940000U, BW_12_5, false },
    { "MURS 4", 154570000U, 154570000U, BW_25,   false },
    { "MURS 5", 154600000U, 154600000U, BW_25,   false },
};

static const presetChannelDef_t weatherChannels[] =
{
    { "WX 1", 162400000U, 162400000U, BW_12_5, true },
    { "WX 2", 162425000U, 162425000U, BW_12_5, true },
    { "WX 3", 162450000U, 162450000U, BW_12_5, true },
    { "WX 4", 162475000U, 162475000U, BW_12_5, true },
    { "WX 5", 162500000U, 162500000U, BW_12_5, true },
    { "WX 6", 162525000U, 162525000U, BW_12_5, true },
    { "WX 7", 162550000U, 162550000U, BW_12_5, true },
};

static const presetChannelDef_t marineChannels[] =
{
    { "M 01A", 156050000U, 156050000U, BW_25, false },
    { "M 05A", 156250000U, 156250000U, BW_25, false },
    { "M 06",  156300000U, 156300000U, BW_25, false },
    { "M 07A", 156350000U, 156350000U, BW_25, false },
    { "M 08",  156400000U, 156400000U, BW_25, false },
    { "M 09",  156450000U, 156450000U, BW_25, false },
    { "M 10",  156500000U, 156500000U, BW_25, false },
    { "M 11",  156550000U, 156550000U, BW_25, false },
    { "M 12",  156600000U, 156600000U, BW_25, false },
    { "M 13",  156650000U, 156650000U, BW_25, false },
    { "M 14",  156700000U, 156700000U, BW_25, false },
    { "M 15",  156750000U, 156750000U, BW_25, false },
    { "M 17",  156850000U, 156850000U, BW_25, false },
    { "M 18A", 156900000U, 156900000U, BW_25, false },
    { "M 19A", 156950000U, 156950000U, BW_25, false },
    { "M 20A", 157000000U, 157000000U, BW_25, false },
    { "M 21A", 157050000U, 157050000U, BW_25, false },
    { "M 22A", 157100000U, 157100000U, BW_25, false },
    { "M 23A", 157150000U, 157150000U, BW_25, false },
    { "M 24",  161800000U, 157200000U, BW_25, false },
    { "M 25",  161850000U, 157250000U, BW_25, false },
    { "M 26",  161900000U, 157300000U, BW_25, false },
    { "M 27",  161950000U, 157350000U, BW_25, false },
    { "M 28",  162000000U, 157400000U, BW_25, false },
    { "M 60A", 156025000U, 156025000U, BW_25, false },
    { "M 61A", 156075000U, 156075000U, BW_25, false },
    { "M 62A", 156125000U, 156125000U, BW_25, false },
    { "M 63A", 156175000U, 156175000U, BW_25, false },
    { "M 64A", 156225000U, 156225000U, BW_25, false },
    { "M 65A", 156275000U, 156275000U, BW_25, false },
    { "M 66A", 156325000U, 156325000U, BW_25, false },
    { "M 67",  156375000U, 156375000U, BW_25, false },
    { "M 68",  156425000U, 156425000U, BW_25, false },
    { "M 69",  156475000U, 156475000U, BW_25, false },
    { "M 71",  156575000U, 156575000U, BW_25, false },
    { "M 72",  156625000U, 156625000U, BW_25, false },
    { "M 73",  156675000U, 156675000U, BW_25, false },
    { "M 74",  156725000U, 156725000U, BW_25, false },
    { "M 77",  156875000U, 156875000U, BW_25, false },
    { "M 78A", 156925000U, 156925000U, BW_25, false },
    { "M 79A", 156975000U, 156975000U, BW_25, false },
    { "M 80A", 157025000U, 157025000U, BW_25, false },
    { "M 81A", 157075000U, 157075000U, BW_25, false },
    { "M 82A", 157125000U, 157125000U, BW_25, false },
    { "M 83A", 157175000U, 157175000U, BW_25, false },
    { "M 84",  161825000U, 157225000U, BW_25, false },
    { "M 85",  161875000U, 157275000U, BW_25, false },
    { "M 86",  161925000U, 157325000U, BW_25, false },
    { "M 88A", 157425000U, 157425000U, BW_25, false },
};

static const presetBankDef_t presetBanks[] =
{
    { PRESET_BANK_FRS,        "FRS",            frsChannels,     sizeof(frsChannels) / sizeof(frsChannels[0]),     false },
    { PRESET_BANK_MURS,       "MURS",           mursChannels,    sizeof(mursChannels) / sizeof(mursChannels[0]),    false },
    { PRESET_BANK_WEATHER_US, "Weather US",     weatherChannels, sizeof(weatherChannels) / sizeof(weatherChannels[0]), false },
    { PRESET_BANK_WEATHER_CA, "Weather Canada", weatherChannels, sizeof(weatherChannels) / sizeof(weatherChannels[0]), false },
    { PRESET_BANK_MARINE,     "Marine",         marineChannels,  sizeof(marineChannels) / sizeof(marineChannels[0]),  true },
};

static const presetBankDef_t *presetChannelsFindBank(uint16_t bankId)
{
    for(uint8_t i = 0; i < (sizeof(presetBanks) / sizeof(presetBanks[0])); i++)
    {
        if(presetBanks[i].bankId == bankId)
            return &presetBanks[i];
    }

    return NULL;
}

static bool presetChannelsIsFrequencySupported(freq_t freq)
{
    return bandplanIsFrequencyInHardwareRange(platform_getHwInfo(), freq);
}

static bool presetChannelsIsBankSupported(const presetBankDef_t *bank)
{
    if(bank == NULL)
        return false;

    for(uint16_t i = 0; i < bank->count; i++)
    {
        if(!presetChannelsIsFrequencySupported(bank->channels[i].rx) ||
           !presetChannelsIsFrequencySupported(bank->channels[i].tx))
            return false;
    }

    return true;
}

static bool presetChannelsIsBankVisible(uint16_t bankId, bandplan_t bandplan)
{
    switch(bankId)
    {
        case PRESET_BANK_FRS:
            return presetChannelsIsBankSupported(&presetBanks[0]);
        case PRESET_BANK_MURS:
            return ((bandplan == BANDPLAN_US) || (bandplan == BANDPLAN_OFF)) &&
                   presetChannelsIsBankSupported(&presetBanks[1]);
        case PRESET_BANK_WEATHER_US:
            return ((bandplan == BANDPLAN_US) || (bandplan == BANDPLAN_OFF)) &&
                   presetChannelsIsBankSupported(&presetBanks[2]);
        case PRESET_BANK_WEATHER_CA:
            return ((bandplan == BANDPLAN_CANADA) || (bandplan == BANDPLAN_OFF)) &&
                   presetChannelsIsBankSupported(&presetBanks[3]);
        case PRESET_BANK_MARINE:
            return presetChannelsIsBankSupported(&presetBanks[4]);
        default:
            return false;
    }
}

uint8_t presetChannelsGetVisibleBankIds(bandplan_t bandplan, uint16_t *bankIds,
                                        uint8_t maxCount)
{
    uint8_t count = 0;

    for(uint8_t i = 0; i < (sizeof(presetBanks) / sizeof(presetBanks[0])); i++)
    {
        if(!presetChannelsIsBankVisible(presetBanks[i].bankId, bandplan))
            continue;

        if((bankIds != NULL) && (count < maxCount))
            bankIds[count] = presetBanks[i].bankId;

        count++;
    }

    return count;
}

bool presetChannelsIsPresetBank(uint16_t bankId)
{
    return presetChannelsFindBank(bankId) != NULL;
}

const char *presetChannelsGetBankName(uint16_t bankId)
{
    const presetBankDef_t *bank = presetChannelsFindBank(bankId);
    if(bank == NULL)
        return NULL;

    return bank->name;
}

uint16_t presetChannelsGetCount(uint16_t bankId)
{
    const presetBankDef_t *bank = presetChannelsFindBank(bankId);
    if(bank == NULL)
        return 0;

    return bank->count;
}

int presetChannelsGetChannel(uint16_t bankId, uint16_t channelIndex, channel_t *channel)
{
    return presetChannelsGetChannelForBandplan(bankId, BANDPLAN_CANADA, channelIndex, channel);
}

int presetChannelsGetChannelForBandplan(uint16_t bankId, bandplan_t bandplan,
                                        uint16_t channelIndex, channel_t *channel)
{
    const presetBankDef_t *bank = presetChannelsFindBank(bankId);
    const presetChannelDef_t *def;

    if((bank == NULL) || (channel == NULL) || (channelIndex >= bank->count))
        return -1;

    def = &bank->channels[channelIndex];

    memset(channel, 0, sizeof(*channel));
    channel->mode = OPMODE_FM;
    channel->bandwidth = def->bandwidth;
    channel->rx_only = (def->rx_only || (bank->tx_requires_off_bandplan && (bandplan != BANDPLAN_OFF))) ? 1 : 0;
    channel->power = powerGetDefaultStoredValue();
    channel->rx_frequency = def->rx;
    channel->tx_frequency = def->tx;
    strncpy(channel->name, def->name, sizeof(channel->name));
    strncpy(channel->descr, bank->name, sizeof(channel->descr));

    return 0;
}
