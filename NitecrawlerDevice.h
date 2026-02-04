/* *******************************************************************************
 * MIT License
 *
 * Copyright (c) 2026 Nico Trost
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * **************************************************************************** */

#ifndef NITECRAWLER_DEVICE_H
#define NITECRAWLER_DEVICE_H

#include "NitecrawlerSDK.h"
#include "NitecrawlerSerialPort.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#define RETURN_IF_ERROR(fct)        \
    {                               \
        NC_ERROR_TYPE stat = fct;   \
        if (stat != NC_SUCCESS)     \
        {                           \
            return stat;            \
        }                           \
    }

namespace Nitecrawler
{
    /**
     * Focuser represents a Nitecrawler Focuser device with its current state.
     */
    struct Focuser
    {
        NC_FOCUSER_CONFIG config;
        NC_FOCUSER_STATUS status;

        Focuser()
        {
            // Some default values
            config.maxStep = 94580;
            config.backlash = 0;
            config.backlashDirection = 0;
            config.reverseDirection = false;
            config.stepRate = 7;
            config.temperatureOffset = 0.0f;

            status.temperatureExt = TEMPERATURE_INVALID;
            status.temperatureDetection = 0;
            status.position = 0;
            status.moving = 0;
            status.micronsPerStep = 0.2667f;
        }
    };

    /**
     * Rotator represents a Nitecrawler Rotator device with its current state.
     */
    struct Rotator
    {
        NC_ROTATOR_CONFIG config;
        NC_ROTATOR_STATUS status;

        Rotator()
        {
            // Some default values
            config.reverseDirection = false;
            config.stepRate = 7;

            status.position = 0;
            status.moving = 0;
            status.stepsPerRevolution = 505960;
            status.stepSize = 360.0f / 505960;
        }
    };

    /**
     * Device represents a Nitecrawler device with its current state.
     */
    struct Device
    {
        bool isOpen;
        int id = -1;
        std::shared_ptr<SerialPort> port;
        std::string portName;
        std::string productModel;
        NC_VERSION version = {0, 0};
        NC_DEVICE_CONFIG config;
        NC_DEVICE_STATUS status;
        std::mutex deviceMutex;
        bool initialized = false; // Track if device has been initialized

        // Reference counting for open/close
        int focuserRefCount = 0; // Count of focuser opens
        int rotatorRefCount = 0; // Count of rotator opens

        // API access control
        enum class AccessMode
        {
            NONE,
            FOCUSER_ONLY,
            ROTATOR_ONLY,
            BOTH
        };
        AccessMode accessMode = AccessMode::NONE;

        Focuser focuser;
        Rotator rotator;

        Device()
        {
            isOpen = false;
            port = std::make_unique<SerialPort>();

            // Some default values
            config.displayBrightness = 150;
            config.sleepBrightness = 0;
            config.voltageOffset = 0.0f;
            config.encoders = 1;
            config.flipDisplay = 0;

            status.voltage = 0.0f;
        }

        /* Simple destructor - nothing to clean up */
        ~Device() = default;
    };

    /**
     * Global device registry mapping device IDs to Device objects.
     */
    extern std::map<int, std::shared_ptr<Device>> g_devices;

    /**
     * Global mutex protecting access to g_devices.
     */
    extern std::mutex g_globalMutex;

} /* namespace Nitecrawler */

#endif /* NITECRAWLER_DEVICE_H */
