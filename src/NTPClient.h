// NTPClient.h

#ifndef _NTPCLIENT_h
#define _NTPCLIENT_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include "neotimer.h"
#include <Preferences.h>

class NTPClient {
public:
    // interval in hours
    NTPClient(int interval = 1) : _NTPTimer(interval * 3600 * 1000), _NTPSync(true), _initialized(false) {}

    void begin(const String& server, const String& timeZone, int32_t timeOffset) {
        _NTPServer = server;
        _TimeZone = timeZone;
        _TimeOffset_sec = timeOffset;

        Serial.println(F("ntp initializing..."));

        // Initialize preferences
        _preferences.begin("ntp", false);

        // Set timezone first (needed for correct time display)
        configTzTime(_TimeZone.c_str(), "");

        // Restore saved time
        restoreTime();

        _NTPSync = true;
        _initialized = true;

        Serial.println(F("OK"));
    }

    void process() {
        if (_NTPTimer.repeat() || _NTPSync) {
            Serial.println(F("ntp processing..."));
            Serial.print(F("    Timezone: ")); Serial.println(_TimeZone);
            Serial.print(F("    NTP server: ")); Serial.println(_NTPServer);

            // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
            configTzTime(_TimeZone.c_str(), _NTPServer.c_str());

            struct tm timeinfo_;
            if (getLocalTime(&timeinfo_)) {
                _NTPSync = false;
                Serial.println(F("NTP time successfully set"));
                char s_[51];
                strftime(s_, 50, "%A, %B %d %Y %H:%M:%S", &timeinfo_);
                Serial.println(s_);
                _isValidTime = true;
            }
        }
    }

    bool isInitialized() const {
        return _initialized;
    }

    bool isValidTime() const {
        return _isValidTime;
    }

    // Set NTP synchronization interval in hours
    void setInterval(int interval) {
        _NTPTimer.start(interval * 3600 * 1000);
    }

    // Call this before reboot to save current time
    void saveTimeBeforeReboot() {
        if (_isValidTime) {
            saveTime();
            Serial.println(F("Time saved before reboot"));
        }
    }

    // Get current time as Unix timestamp
    time_t getEpochTime() const {
        return time(nullptr);
    }

    // Get formatted time string (HH:MM:SS)
    String getTimeString() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "00:00:00";
        }
        char buffer[9];
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
        return String(buffer);
    }

    // Get formatted date string (DD.MM.YYYY)
    String getDateString() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "01.01.1970";
        }
        char buffer[11];
        strftime(buffer, sizeof(buffer), "%d.%m.%Y", &timeinfo);
        return String(buffer);
    }

    // Get formatted date and time string (DD.MM.YYYY HH:MM:SS)
    String getDateTimeString() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "01.01.1970 00:00:00";
        }
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        return String(buffer);
    }

    // Get ISO 8601 formatted string (YYYY-MM-DDTHH:MM:SS)
    String getISOString() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "1970-01-01T00:00:00";
        }
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
        return String(buffer);
    }

    // Get current hour (0-23)
    int getHour() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 0;
        }
        return timeinfo.tm_hour;
    }

    // Get current minute (0-59)
    int getMinute() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 0;
        }
        return timeinfo.tm_min;
    }

    // Get current second (0-59)
    int getSecond() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 0;
        }
        return timeinfo.tm_sec;
    }

    // Get current day (1-31)
    int getDay() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 1;
        }
        return timeinfo.tm_mday;
    }

    // Get current month (1-12)
    int getMonth() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 1;
        }
        return timeinfo.tm_mon + 1; // tm_mon is 0-11
    }

    // Get current year (e.g., 2026)
    int getYear() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 1970;
        }
        return timeinfo.tm_year + 1900; // tm_year is years since 1900
    }

    // Get day of week (0=Sunday, 6=Saturday)
    int getDayOfWeek() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 0;
        }
        return timeinfo.tm_wday;
    }

    // Get day of week as string
    String getDayOfWeekString() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "Unknown";
        }
        char buffer[10];
        strftime(buffer, sizeof(buffer), "%A", &timeinfo);
        return String(buffer);
    }

    // Get minutes since midnight (for scene scheduling)
    int getMinutesSinceMidnight() const {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return 0;
        }
        return timeinfo.tm_hour * 60 + timeinfo.tm_min;
    }

private:
    void saveTime() {
        struct tm timeinfo_;
        if (getLocalTime(&timeinfo_)) {
            time_t now = mktime(&timeinfo_);

            // Store timestamp only (no uptime needed)
            _preferences.putULong("timestamp", (unsigned long)now);

            Serial.print(F("Time saved to preferences: "));
            Serial.println((unsigned long)now);
        }
    }

    void restoreTime() {
        unsigned long savedTimestamp = _preferences.getULong("timestamp", 0);

        if (savedTimestamp > 0) {
            // After reboot, use saved time plus current uptime as approximation
            unsigned long currentUptime = millis();
            unsigned long elapsedSeconds = currentUptime / 1000;

            // Calculate restored time
            time_t restoredTime = (time_t)(savedTimestamp + elapsedSeconds);

            struct timeval tv = { .tv_sec = restoredTime, .tv_usec = 0 };
            settimeofday(&tv, NULL);

            _isValidTime = true;

            Serial.println(F("Time restored from preferences"));
            struct tm timeinfo_;
            if (getLocalTime(&timeinfo_)) {
                char s_[51];
                strftime(s_, 50, "%A, %B %d %Y %H:%M:%S", &timeinfo_);
                Serial.print(F("Restored time: "));
                Serial.println(s_);
            }
        }
        else {
            Serial.println(F("No saved time found in preferences"));
        }
    }

    String _NTPServer;
    String _TimeZone;
    int32_t _TimeOffset_sec;
    Neotimer _NTPTimer;
    bool _NTPSync;
    bool _initialized;
    bool _isValidTime = false;
    Preferences _preferences;
};

#endif

