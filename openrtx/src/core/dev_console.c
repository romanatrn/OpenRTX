/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/dev_console.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/datetime.h"
#include "core/state.h"
#include "interfaces/delays.h"
#include "interfaces/platform.h"

#ifndef PLATFORM_LINUX
#include "drivers/usb_vcom.h"
#endif

#define DEV_CONSOLE_MAX_LINES 64
#define DEV_CONSOLE_USB_QUEUE_LEN 16

static pthread_mutex_t devConsoleMutex = PTHREAD_MUTEX_INITIALIZER;
static char devConsoleLines[DEV_CONSOLE_MAX_LINES][DEV_CONSOLE_LINE_LEN];
static char devConsoleUsbQueue[DEV_CONSOLE_USB_QUEUE_LEN][DEV_CONSOLE_USB_LINE_LEN + 2];
static size_t devConsoleStart = 0;
static size_t devConsoleCount = 0;
static size_t devConsoleUsbRead = 0;
static size_t devConsoleUsbWrite = 0;
static size_t devConsoleUsbCount = 0;
static bool devConsoleUsbExport = false;
static bool devConsoleReady = false;

static size_t devConsoleWrappedRows(const char *line, size_t wrapWidth)
{
    size_t lineLen;

    if((line == NULL) || (wrapWidth == 0))
        return 0;

    lineLen = strlen(line);
    if(lineLen == 0)
        return 1;

    return ((lineLen - 1) / wrapWidth) + 1;
}

static const char *devConsoleLevelName(devLogLevel_t level)
{
    switch(level)
    {
        case DEVLOG_ERROR: return "ERR";
        case DEVLOG_WARN:  return "WRN";
        case DEVLOG_INFO:  return "INF";
        case DEVLOG_DEBUG: return "DBG";
        default:           return "LOG";
    }
}

void devConsole_init(void)
{
    devConsoleStart = 0;
    devConsoleCount = 0;
    devConsoleUsbRead = 0;
    devConsoleUsbWrite = 0;
    devConsoleUsbCount = 0;
    devConsoleUsbExport = false;
    devConsoleReady = true;
}

void devConsole_setUsbExportEnabled(bool enabled)
{
    pthread_mutex_lock(&devConsoleMutex);
    devConsoleUsbExport = enabled;
    pthread_mutex_unlock(&devConsoleMutex);
}

bool devConsole_getUsbExportEnabled(void)
{
    bool enabled;

    pthread_mutex_lock(&devConsoleMutex);
    enabled = devConsoleUsbExport;
    pthread_mutex_unlock(&devConsoleMutex);

    return enabled;
}

void devConsole_clear(void)
{
    pthread_mutex_lock(&devConsoleMutex);
    devConsoleStart = 0;
    devConsoleCount = 0;
    devConsoleUsbRead = 0;
    devConsoleUsbWrite = 0;
    devConsoleUsbCount = 0;
    pthread_mutex_unlock(&devConsoleMutex);
}

void devConsole_log(devLogLevel_t level, const char *tag, const char *fmt, ...)
{
    long long tick;
    datetime_t now;
    datetime_t localTime;
    char message[DEV_CONSOLE_LINE_LEN];
    char line[DEV_CONSOLE_LINE_LEN];
    char usbLine[DEV_CONSOLE_USB_LINE_LEN + 2];
    size_t prefixLen;
    bool haveDateTime = false;

    if(!devConsoleReady)
        devConsole_init();

    tick = getTick();
    now = platform_getCurrentTime();

    if((now.year >= 24) && (now.month >= 1) && (now.month <= 12) &&
       (now.date >= 1) && (now.date <= 31) &&
       (now.hour >= 0) && (now.hour <= 23) &&
       (now.minute >= 0) && (now.minute <= 59) &&
       (now.second >= 0) && (now.second <= 59))
    {
        localTime = utcToLocalTime(now, state.settings.utc_timezone);
        haveDateTime = true;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    snprintf(line, sizeof(line), "%s %.6s:",
             devConsoleLevelName(level),
             (tag != NULL) ? tag : "SYS");
    strncat(line, " ", sizeof(line) - strlen(line) - 1);
    strncat(line, message, sizeof(line) - strlen(line) - 1);

    pthread_mutex_lock(&devConsoleMutex);

    size_t writeIndex = (devConsoleStart + devConsoleCount) % DEV_CONSOLE_MAX_LINES;
    if(devConsoleCount == DEV_CONSOLE_MAX_LINES)
    {
        writeIndex = devConsoleStart;
        devConsoleStart = (devConsoleStart + 1) % DEV_CONSOLE_MAX_LINES;
    }
    else
    {
        devConsoleCount++;
    }

    strncpy(devConsoleLines[writeIndex], line, DEV_CONSOLE_LINE_LEN - 1);
    devConsoleLines[writeIndex][DEV_CONSOLE_LINE_LEN - 1] = '\0';

    if(devConsoleUsbExport)
    {
        if(haveDateTime)
        {
            prefixLen = snprintf(usbLine, sizeof(usbLine), "20%02u-%02u-%02u %02d:%02d:%02d ",
                                 (unsigned int) localTime.year,
                                 (unsigned int) localTime.month,
                                 (unsigned int) localTime.date,
                                 localTime.hour,
                                 localTime.minute,
                                 localTime.second);
            if(prefixLen < sizeof(usbLine))
                snprintf(usbLine + prefixLen, sizeof(usbLine) - prefixLen, "%s\r\n", line);
        }
        else
        {
            snprintf(usbLine, sizeof(usbLine), "%08lld %s\r\n", tick, line);
        }

        if(devConsoleUsbCount == DEV_CONSOLE_USB_QUEUE_LEN)
        {
            devConsoleUsbRead = (devConsoleUsbRead + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
            devConsoleUsbCount--;
        }

        snprintf(devConsoleUsbQueue[devConsoleUsbWrite],
                 sizeof(devConsoleUsbQueue[devConsoleUsbWrite]),
                 "%s", usbLine);
        devConsoleUsbWrite = (devConsoleUsbWrite + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
        devConsoleUsbCount++;
    }

    pthread_mutex_unlock(&devConsoleMutex);
}

#ifndef PLATFORM_LINUX
void devConsole_process(void)
{
    char usbLine[DEV_CONSOLE_USB_LINE_LEN + 2];

    if((devConsoleReady == false) || (vcom_isConnected() == false))
        return;

    pthread_mutex_lock(&devConsoleMutex);

    if((devConsoleUsbExport == false) || (devConsoleUsbCount == 0))
    {
        pthread_mutex_unlock(&devConsoleMutex);
        return;
    }

    snprintf(usbLine, sizeof(usbLine), "%s", devConsoleUsbQueue[devConsoleUsbRead]);
    pthread_mutex_unlock(&devConsoleMutex);

    if(vcom_writeBlockNonblocking(usbLine, strlen(usbLine)) <= 0)
        return;

    pthread_mutex_lock(&devConsoleMutex);
    if(devConsoleUsbCount > 0)
    {
        devConsoleUsbRead = (devConsoleUsbRead + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
        devConsoleUsbCount--;
    }
    pthread_mutex_unlock(&devConsoleMutex);
}
#else
void devConsole_process(void)
{
}
#endif

size_t devConsole_getLineCount(void)
{
    size_t count;

    pthread_mutex_lock(&devConsoleMutex);
    count = devConsoleCount;
    pthread_mutex_unlock(&devConsoleMutex);

    return count;
}

bool devConsole_getLine(size_t index, char *buf, size_t len)
{
    bool valid = false;

    if((buf == NULL) || (len == 0))
        return false;

    pthread_mutex_lock(&devConsoleMutex);

    if(index < devConsoleCount)
    {
        size_t realIndex = (devConsoleStart + index) % DEV_CONSOLE_MAX_LINES;
        snprintf(buf, len, "%s", devConsoleLines[realIndex]);
        valid = true;
    }

    pthread_mutex_unlock(&devConsoleMutex);

    return valid;
}

size_t devConsole_getDisplayRowCount(size_t wrapWidth)
{
    size_t rowCount = 0;

    if(wrapWidth == 0)
        return 0;

    pthread_mutex_lock(&devConsoleMutex);

    for(size_t i = 0; i < devConsoleCount; i++)
    {
        size_t realIndex = (devConsoleStart + i) % DEV_CONSOLE_MAX_LINES;
        rowCount += devConsoleWrappedRows(devConsoleLines[realIndex], wrapWidth);
    }

    pthread_mutex_unlock(&devConsoleMutex);

    return rowCount;
}

bool devConsole_getDisplayRow(size_t rowIndex, size_t wrapWidth, char *buf, size_t len)
{
    size_t cursor = 0;

    if((buf == NULL) || (len == 0) || (wrapWidth == 0))
        return false;

    pthread_mutex_lock(&devConsoleMutex);

    for(size_t i = 0; i < devConsoleCount; i++)
    {
        size_t realIndex = (devConsoleStart + i) % DEV_CONSOLE_MAX_LINES;
        const char *line = devConsoleLines[realIndex];
        size_t rows = devConsoleWrappedRows(line, wrapWidth);

        if(rowIndex < (cursor + rows))
        {
            size_t offset = (rowIndex - cursor) * wrapWidth;
            snprintf(buf, len, "%.*s", (int) wrapWidth, line + offset);
            pthread_mutex_unlock(&devConsoleMutex);
            return true;
        }

        cursor += rows;
    }

    pthread_mutex_unlock(&devConsoleMutex);
    return false;
}
