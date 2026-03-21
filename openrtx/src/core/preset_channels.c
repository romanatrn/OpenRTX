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

static const presetBankDef_t presetBanks[] =
{
    { PRESET_BANK_FRS,        "FRS",            frsChannels,     sizeof(frsChannels) / sizeof(frsChannels[0]) },
    { PRESET_BANK_MURS,       "MURS",           mursChannels,    sizeof(mursChannels) / sizeof(mursChannels[0]) },
    { PRESET_BANK_WEATHER_US, "Weather US",     weatherChannels, sizeof(weatherChannels) / sizeof(weatherChannels[0]) },
    { PRESET_BANK_WEATHER_CA, "Weather Canada", weatherChannels, sizeof(weatherChannels) / sizeof(weatherChannels[0]) },
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
            return (bandplan == BANDPLAN_US) && presetChannelsIsBankSupported(&presetBanks[1]);
        case PRESET_BANK_WEATHER_US:
            return (bandplan == BANDPLAN_US) && presetChannelsIsBankSupported(&presetBanks[2]);
        case PRESET_BANK_WEATHER_CA:
            return (bandplan == BANDPLAN_CANADA) && presetChannelsIsBankSupported(&presetBanks[3]);
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
    const presetBankDef_t *bank = presetChannelsFindBank(bankId);
    const presetChannelDef_t *def;

    if((bank == NULL) || (channel == NULL) || (channelIndex >= bank->count))
        return -1;

    def = &bank->channels[channelIndex];

    memset(channel, 0, sizeof(*channel));
    channel->mode = OPMODE_FM;
    channel->bandwidth = def->bandwidth;
    channel->rx_only = def->rx_only ? 1 : 0;
    channel->power = powerGetDefaultStoredValue();
    channel->rx_frequency = def->rx;
    channel->tx_frequency = def->tx;
    strncpy(channel->name, def->name, sizeof(channel->name));
    strncpy(channel->descr, bank->name, sizeof(channel->descr));

    return 0;
}
