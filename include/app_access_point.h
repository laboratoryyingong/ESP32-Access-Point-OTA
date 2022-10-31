#ifndef APP_ACCESS_POINT_H
#define APP_ACCESS_POINT_H

/**
 * @file access_point.h
 *
 * @brief setup esp32 as a access point and use phone to conenct and send commands
 *
 * @author Max Gong
 * Contact: max.gong@nymet.com.au
 *
 */

#include <Arduino.h>
#include <WiFi.h>

#define AP_PORT 80

class AccessPoint
{
public:
    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    static void Init();
    static void Loop();
    static String _HumanReadableSize(const size_t bytes);

protected:
private:
};

#endif