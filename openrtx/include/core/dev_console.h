#ifndef DEV_CONSOLE_H
#define DEV_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEV_CONSOLE_LINE_LEN 80
#define DEV_CONSOLE_USB_LINE_LEN 96

typedef enum
{
    DEVLOG_ERROR = 0,
    DEVLOG_WARN,
    DEVLOG_INFO,
    DEVLOG_DEBUG
} devLogLevel_t;

void devConsole_init(void);
void devConsole_setUsbExportEnabled(bool enabled);
bool devConsole_getUsbExportEnabled(void);
void devConsole_clear(void);
void devConsole_log(devLogLevel_t level, const char *tag, const char *fmt, ...);
void devConsole_process(void);
size_t devConsole_getLineCount(void);
bool devConsole_getLine(size_t index, char *buf, size_t len);
size_t devConsole_getDisplayRowCount(size_t wrapWidth);
bool devConsole_getDisplayRow(size_t rowIndex, size_t wrapWidth, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
