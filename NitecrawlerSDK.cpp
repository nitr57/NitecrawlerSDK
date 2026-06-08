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

#include "NitecrawlerSDK.h"
#include "NitecrawlerLogging.h"
#include "NitecrawlerSerialPort.h"
#include "NitecrawlerDevice.h"
#include "NitecrawlerProtocol.h"
#include <map>
#include <vector>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <thread>
#include <mutex>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <chrono>
#include <queue>
#include <string>
#include <condition_variable>
#include <libudev.h>

using namespace Nitecrawler;

/* ============================================================================
 * DEVICE SDK API IMPLEMENTATION
 * ============================================================================ */
NCAPI NC_ERROR_TYPE NCGetProductModel(int id, char *model)
{
    if (!model)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    auto device = it->second;
    strncpy(model, device->productModel.c_str(), NC_NAME_LEN - 1);
    model[NC_NAME_LEN - 1] = '\0';
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCGetVersion(int id, NC_VERSION *version)
{
    if (!version)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    auto device = it->second;

    version->firmware = device->version.firmware;
    version->serial = device->version.serial;

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCGetSDKVersion(char *version)
{
    if (!version)
    {
        return NC_ERROR_NULL_POINTER;
    }

    strncpy(version, "1.1.2", NC_VERSION_LEN - 1);
    version[NC_VERSION_LEN - 1] = '\0';

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCGetStatus(int id, NC_DEVICE_STATUS *status)
{
    if (!status)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    auto device = it->second;

    // Query voltage
    RETURN_IF_ERROR(QueryVoltage(device));

    status->voltage = device->status.voltage;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCGetConfig(int id, NC_DEVICE_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    *config = it->second->config;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCSetConfig(int id, NC_DEVICE_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        device = it->second;
    }
    // Global lock released here before serial operations

    // Protect device serial operations with device-level mutex
    std::lock_guard<std::mutex> deviceLock(device->deviceMutex);

    if (config->mask & MASK_DEVICE_BRIGHTNESS)
    {
        // Validate brightness value (0-255)
        if (config->displayBrightness < 0 || config->displayBrightness > 255)
        {
            return NC_ERROR_INVALID_PARAMETER;
        }

        // Send brightness command to device using: PD ###
        char response[32];
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "PD %03d#", config->displayBrightness);

        NC_INFO("Setting display brightness to %d with command: %s", config->displayBrightness, cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->config.displayBrightness = config->displayBrightness;
    }

    if (config->mask & MASK_DEVICE_SLEEP_BRIGHTNESS)
    {
        // Validate sleep brightness value (0-255)
        if (config->sleepBrightness < 0 || config->sleepBrightness > 255)
        {
            return NC_ERROR_INVALID_PARAMETER;
        }

        // Send sleep brightness command to device using: PL ###
        char response[32];
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "PL %03d#", config->sleepBrightness);

        NC_INFO("Setting sleep brightness to %d with command: %s", config->sleepBrightness, cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->config.sleepBrightness = config->sleepBrightness;
    }

    if (config->mask & MASK_DEVICE_VOLTAGE_OFFSET)
    {
        // Validate voltage offset value (-30.0 to 30.0 in V)
        if (config->voltageOffset < -30.0 || config->voltageOffset > 30.0)
        {
            return NC_ERROR_INVALID_PARAMETER;
        }

        // Send voltage offset command to device using: Pv %03d#
        // Device expects the value in 0.1V units (multiply float by 10)
        char response[32];
        char cmd[32];
        int offsetValue = (int)(config->voltageOffset * 10);
        snprintf(cmd, sizeof(cmd), "Pv %03d#", offsetValue);

        NC_INFO("Setting voltage offset to %.1fV with command: %s", config->voltageOffset, cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->config.voltageOffset = config->voltageOffset;
    }

    if (config->mask & MASK_DEVICE_ENCODERS)
    {
        // Send encoders command to device using: PE %02d#
        // Device expects: 01 for enabled, 00 for disabled
        char response[32];
        char cmd[32];
        int encoderValue = config->encoders ? 1 : 0;
        snprintf(cmd, sizeof(cmd), "PE %02d#", encoderValue);

        NC_INFO("Setting encoders to %s with command: %s", (encoderValue ? "Enabled" : "Disabled"), cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->config.encoders = config->encoders;
    }

    if (config->mask & MASK_DEVICE_FLIP_DISPLAY)
    {
        // Send flip display command to device using format: C 20 %02d#
        // Device expects: 01 for on, 00 for off
        char response[32];
        char cmd[32];
        int flipValue = config->flipDisplay ? 1 : 0;
        snprintf(cmd, sizeof(cmd), "C 20 %02d#", flipValue);

        NC_INFO("Setting display flip to %s with command: %s", (flipValue ? "On" : "Off"), cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->config.flipDisplay = config->flipDisplay;
    }

    return NC_SUCCESS;
}

static NC_ERROR_TYPE NCOpen(std::shared_ptr<Device> device)
{
    if (device->isOpen)
    {
        // Device already exists (either from focuser or rotator)
        NC_INFO("Device %s already open, incrementing device ref count", device->portName.c_str());
    }
    else
    {
        if (!device->port)
        {
            NC_DEBUG("Creating new SerialPort instance");
            device->port = std::make_shared<SerialPort>();
        }

        NC_DEBUG("Attempting to open port %s", device->portName.c_str());
        if (!device->port->Open(device->portName.c_str()))
        {
            NC_ERROR("Failed to open port");
            return NC_ERROR_COMMUNICATION;
        }

        NC_DEBUG("Port opened successfully, performing handshake");

        // Perform handshake with retry mechanism
        char response[32];
        if (SendAndWaitForReplyWithRetry(device, "#", response, 32) != NC_SUCCESS)
        {
            NC_ERROR("Handshake failed");
            device->port->Close();
            return NC_ERROR_COMMUNICATION;
        }

        // Check for handshake reply
        if (strncmp(response, "NACK", 4) != 0)
        {
            NC_ERROR("Handshake failed");
            device->port->Close();
            return NC_ERROR_COMMUNICATION;
        }

        // Initialize device properties from hardware
        InitializeDeviceProperties(device);
        device->isOpen = true;
    }

    return NC_SUCCESS;
}

/* ============================================================================
 * FOCUSER SDK API IMPLEMENTATION
 * ============================================================================ */
NCAPI NC_ERROR_TYPE NCFocuserScan(int *number, int *ids)
{
    NC_INFO("Scanning for Nitecrawler devices");
    if (!number || !ids)
    {
        NC_ERROR("Invalid pointer parameters");
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    // Build a set of already-connected port names
    std::map<std::string, int> connectedPorts; // port name -> device id
    for (const auto& pair : g_devices)
    {
        if (!pair.second->portName.empty())
        {
            connectedPorts[pair.second->portName] = pair.first;
            NC_DEBUG("Found already-connected device on port: %s (id=%d)", pair.second->portName.c_str(), pair.first);
        }
    }

    int count = 0;

    /* Create udev context */
    struct udev *udev = udev_new();
    if (!udev)
    {
        return NC_ERROR_COMMUNICATION;
    }

    /* Create enumeration for tty devices */
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    if (!enumerate)
    {
        udev_unref(udev);
        return NC_ERROR_COMMUNICATION;
    }

    /* Filter for tty subsystem */
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    char response[32];

    /* Iterate through all tty devices */
    udev_list_entry_foreach(entry, devices)
    {
        if (count >= NC_MAX_NUM)
            break;

        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *device = udev_device_new_from_syspath(udev, path);
        if (!device)
        {
            continue;
        }

        /* Get the parent USB device */
        struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(
            device, "usb", "usb_device");

        if (!parent)
        {
            udev_device_unref(device);
            continue;
        }

        /* Check VID and PID for FTDI (0403:6015) */
        const char *vid = udev_device_get_sysattr_value(parent, "idVendor");
        const char *pid = udev_device_get_sysattr_value(parent, "idProduct");

        if (!vid || !pid)
        {
            udev_device_unref(device);
            continue;
        }

        NC_DEBUG("Found device with VID:%s PID:%s", vid, pid);

        if (strcmp(vid, "0403") != 0 || strcmp(pid, "6015") != 0)
        {
            udev_device_unref(device);
            continue;
        }

        /* Get the device node (e.g., /dev/ttyUSB0) */
        const char *deviceNode = udev_device_get_devnode(device);
        if (!deviceNode)
        {
            udev_device_unref(device);
            continue;
        }

        NC_DEBUG("Trying to open device: %s", deviceNode);

        // Check if this port is already connected
        auto connectedIt = connectedPorts.find(deviceNode);
        if (connectedIt != connectedPorts.end())
        {
            NC_INFO("Device on port %s is already connected (id=%d), skipping handshake", deviceNode, connectedIt->second);
            ids[count] = connectedIt->second;
            count++;
            udev_device_unref(device);
            continue;
        }

        /* Try to open the port */
        auto port = std::make_shared<SerialPort>();
        if (port->Open(deviceNode))
        {
            NC_DEBUG("Port opened, flushing and sending command...");

            auto tempDevice = std::make_shared<Device>();
            tempDevice->port = port;
            tempDevice->portName = deviceNode;

            // Perform handshake with retry mechanism
            NC_ERROR_TYPE stat = SendAndWaitForReplyWithRetry(tempDevice, "#", response, 32);
            if(stat != NC_SUCCESS)
            {
                return stat;
            }

            // Check for handshake reply
            if (strncmp(response, "NACK", 4) == 0)
            {
                NC_DEBUG("Valid device found!");

                // Fetch serial number, firmware and product type
                QueryProductModel(tempDevice);
                QueryFirmwareVersion(tempDevice);
                QuerySerialNumber(tempDevice);

                /* Valid device found - close port */
                port->Close();
                int id = count;
                g_devices[id] = tempDevice;
                ids[count] = id;
                count++;
            }
            else
            {
                NC_DEBUG("No response from device");
                /* Not a valid device, close port */
                port->Close();
            }
        }
        else
        {
            NC_DEBUG("Failed to open port %s", deviceNode);
        }

        udev_device_unref(device);
    }

    /* Clean up udev resources */
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    *number = count;

    NC_INFO("Scan complete: found %d device(s)", count);
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserOpen(int id)
{
    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);
        NC_INFO("Opening focuser on device %d", id);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            NC_ERROR("Device id=%d not found", id);
            return NC_ERROR_INVALID_ID;
        }

        device = it->second;
        NC_DEBUG("Found device, portName=%s", device->portName.c_str());

        RETURN_IF_ERROR(NCOpen(device));

        // Increment focuser reference count
        device->focuserRefCount++;

        // Update access mode
        if (device->focuserRefCount == 1)
        {
            if (device->rotatorRefCount == 0)
            {
                device->accessMode = Device::AccessMode::FOCUSER_ONLY;
            }
            else
            {
                device->accessMode = Device::AccessMode::BOTH;
            }
        }
    }
    // Global lock released here before long serial operations

    // Perform serial operations with device-level synchronization
    std::lock_guard<std::mutex> deviceLock(device->deviceMutex);
    QueryMotorStepRate(device, 1);
    QueryTemperatureOffset(device);

    NC_INFO("Device %d focuser opened (ref count: %d)", id, device->focuserRefCount);
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserClose(int id)
{
    NC_INFO("Closing focuser on device %d", id);
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        NC_ERROR("Device %d not found", id);
        return NC_ERROR_INVALID_ID;
    }

    auto device = it->second;

    if(device->focuserRefCount == 0)
    {
        // Nothing need to be done
        return NC_SUCCESS;
    }

    // Decrement focuser reference count
    device->focuserRefCount--;
    if (device->focuserRefCount < 0)
    {
        device->focuserRefCount = 0; // Safeguard
    }

    // Update access mode
    if (device->focuserRefCount == 0)
    {
        if (device->rotatorRefCount == 0)
        {
            device->accessMode = Device::AccessMode::NONE;
            // Close the port only if both are closed
            device->port->Close();
            device->isOpen = false;
            NC_INFO("Device %d completely closed", id);
        }
        else
        {
            device->accessMode = Device::AccessMode::ROTATOR_ONLY;
            NC_INFO("Device %d focuser closed, rotator still open", id);
        }
    }

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserGetConfig(int id, NC_FOCUSER_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        NC_ERROR("Current access state: %s", it->second->accessMode == Device::AccessMode::FOCUSER_ONLY ? "focuser only" : (it->second->accessMode == Device::AccessMode::ROTATOR_ONLY ? "rotator only" : "both"));
        return NC_ERROR_INVALID_STATE;
    }

    *config = it->second->focuser.config;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserSetConfig(int id, NC_FOCUSER_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        // Check access mode - allow if focuser is open or both are open
        if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
            it->second->accessMode != Device::AccessMode::BOTH)
        {
            return NC_ERROR_INVALID_STATE;
        }

        device = it->second;

        if (config->mask & MASK_FOCUSER_MAX_STEP)
        {
            device->focuser.config.maxStep = config->maxStep;
        }
        if (config->mask & MASK_FOCUSER_BACKLASH)
        {
            device->focuser.config.backlash = config->backlash;
        }
        if (config->mask & MASK_FOCUSER_BACKLASH_DIRECTION)
        {
            device->focuser.config.backlashDirection = config->backlashDirection;
        }
        if (config->mask & MASK_FOCUSER_REVERSE_DIRECTION)
        {
            device->focuser.config.reverseDirection = config->reverseDirection;
        }
    }
    // Lock released here before sending command

    if (config->mask & MASK_FOCUSER_STEP_RATE)
    {
        // Use helper function to set step rate
        NC_ERROR_TYPE rc = SetMotorStepRate(device, 1, config->stepRate);
        if (rc != NC_SUCCESS)
        {
            return rc;
        }
    }

    if (config->mask & MASK_FOCUSER_TEMPERATURE_OFFSET)
    {
        // Validate temperature offset value (-15.0 to 15.0 in °C)
        if (config->temperatureOffset < -15.0 || config->temperatureOffset > 15.0)
        {
            return NC_ERROR_INVALID_PARAMETER;
        }

        // Send temperature offset command to device using: Pt %03d#
        // Device expects the value in 0.1°C units (multiply float by 10)
        char response[32];
        char cmd[32];
        int offsetValue = (int)(config->temperatureOffset * 10);
        snprintf(cmd, sizeof(cmd), "Pt %03d#", offsetValue);

        NC_DEBUG("Setting focuser temperature offset to %.1f°C with command: %s", config->temperatureOffset, cmd);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        // Update local config after successful send
        device->focuser.config.temperatureOffset = config->temperatureOffset;
    }

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserGetStatus(int id, NC_FOCUSER_STATUS *status)
{
    if (!status)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        // Check access mode - allow if focuser is open or both are open
        if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
            it->second->accessMode != Device::AccessMode::BOTH)
        {
NC_ERROR("Current access state: %s", it->second->accessMode == Device::AccessMode::FOCUSER_ONLY ? "focuser only" : (it->second->accessMode == Device::AccessMode::ROTATOR_ONLY ? "rotator only" : "both"));
            return NC_ERROR_INVALID_STATE;
        }

        device = it->second;
    }
    // Global lock released here before serial operations

    // Protect device serial operations with device-level mutex
    std::lock_guard<std::mutex> deviceLock(device->deviceMutex);

    // Query position and moving status
    RETURN_IF_ERROR(QueryMotorPosition(device, 1, device->focuser.status.position));
    RETURN_IF_ERROR(QueryMotorState(device, 1, device->focuser.status.moving));

    // Query temperature with "GT#"
    RETURN_IF_ERROR(QueryTemperature(device));

    // According to the manufacturer
    device->focuser.status.micronsPerStep = 0.2667f;

    *status = device->focuser.status;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserSyncPosition(int id, int position)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;
    NC_ERROR_TYPE rc = SetMotorSyncPosition(device, 1, position);
    if (rc == NC_SUCCESS)
    {
        device->focuser.status.position = position;
    }
    return rc;
}

NCAPI NC_ERROR_TYPE NCFocuserMove(int id, int step)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;
    int newPosition = device->focuser.status.position + step;

    if (newPosition < 0 || newPosition > device->focuser.config.maxStep)
    {
        return NC_ERROR_INVALID_PARAMETER;
    }

    // Use helper to move to new position
    NC_ERROR_TYPE rc = MoveMotorTo(device, 1, newPosition);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    // Then start the motor with relative movement
    rc = MoveMotor(device, 1, step);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    device->focuser.status.position = newPosition;
    device->focuser.status.moving = 1;

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserMoveTo(int id, int position)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;

    if (position < 0 || position > device->focuser.config.maxStep)
    {
        return NC_ERROR_INVALID_PARAMETER;
    }

    // Use helper to move to position
    NC_ERROR_TYPE rc = MoveMotorTo(device, 1, position);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    // Then start the motor
    rc = MoveMotor(device, 1, 0);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    device->focuser.status.position = position;
    device->focuser.status.moving = 1;

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCFocuserStopMove(int id)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;

    // Use helper to stop motor
    NC_ERROR_TYPE rc = StopMotor(device, 1);
    if (rc == NC_SUCCESS)
    {
        device->focuser.status.moving = 0;
    }

    return rc;
}

NCAPI NC_ERROR_TYPE NCFocuserFindHome(int id)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if focuser is open or both are open
    if (it->second->accessMode != Device::AccessMode::FOCUSER_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;

    // Use helper to find home (motorType = 1 for focuser)
    return FindMotorHome(device, 1);
}

/* ============================================================================
 * ROTATOR SDK API IMPLEMENTATION
 * ============================================================================ */

NCAPI NC_ERROR_TYPE NCRotatorScan(int *number, int *ids)
{
    return NCFocuserScan(number, ids);
}

NCAPI NC_ERROR_TYPE NCRotatorOpen(int id)
{
    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);
        NC_INFO("Opening rotator on device %d", id);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            NC_ERROR("Device id=%d not found", id);
            return NC_ERROR_INVALID_ID;
        }

        device = it->second;
        NC_DEBUG("Found device, portName=%s", device->portName.c_str());

        RETURN_IF_ERROR(NCOpen(device));

        // Increment rotator reference count
        device->rotatorRefCount++;

        // Update access mode
        if (device->rotatorRefCount == 1)
        {
            if (device->focuserRefCount == 0)
            {
                device->accessMode = Device::AccessMode::ROTATOR_ONLY;
            }
            else
            {
                device->accessMode = Device::AccessMode::BOTH;
            }
        }
    }
    // Global lock released here before long serial operations

    // Perform serial operations with device-level synchronization
    std::lock_guard<std::mutex> deviceLock(device->deviceMutex);
    QueryMotorStepRate(device, 2);

    // Initialize position by querying current position from device
    int positionSteps = 0;
    int moving = 0;

    RETURN_IF_ERROR(QueryMotorPosition(device, 2, positionSteps));
    RETURN_IF_ERROR(QueryMotorState(device, 2, moving));

    // Get corrected position in degree
    device->rotator.status.position = GetRotatorAngleFromSteps(positionSteps, device->rotator.status.stepsPerRevolution, device->rotator.config.reverseDirection);

    NC_INFO("Device %d rotator opened (ref count: %d)", id, device->rotatorRefCount);
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorClose(int id)
{
    NC_INFO("Closing rotator on device %d", id);
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        NC_ERROR("Device %d not found", id);
        return NC_ERROR_INVALID_ID;
    }

    auto device = it->second;

    if(device->rotatorRefCount == 0)
    {
        // Nothing need to be done
        return NC_SUCCESS;
    }

    // Decrement rotator reference count
    device->rotatorRefCount--;
    if (device->rotatorRefCount < 0)
    {
        device->rotatorRefCount = 0; // Safeguard
    }

    // Update access mode
    if (device->rotatorRefCount == 0)
    {
        if (device->focuserRefCount == 0)
        {
            device->accessMode = Device::AccessMode::NONE;
            // Close the port only if both are closed
            device->port->Close();
            device->isOpen = false;
            NC_INFO("Device %d completely closed", id);
        }
        else
        {
            device->accessMode = Device::AccessMode::FOCUSER_ONLY;
            NC_INFO("Device %d rotator closed, focuser still open", id);
        }
    }

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorGetConfig(int id, NC_ROTATOR_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if rotator is open or both are open
    if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    *config = it->second->rotator.config;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorSetConfig(int id, NC_ROTATOR_CONFIG *config)
{
    if (!config)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        // Check access mode - allow if rotator is open or both are open
        if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
            it->second->accessMode != Device::AccessMode::BOTH)
        {
            return NC_ERROR_INVALID_STATE;
        }

        device = it->second;

        if (config->mask & MASK_ROTATOR_REVERSE_DIRECTION)
        {
            device->rotator.config.reverseDirection = config->reverseDirection;
        }
    }
    // Lock released here before sending command

    if (config->mask & MASK_ROTATOR_STEP_RATE)
    {
        // Use helper function to set step rate
        NC_ERROR_TYPE rc = SetMotorStepRate(device, 2, config->stepRate);
        if (rc != NC_SUCCESS)
        {
            return rc;
        }
    }

    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorGetStatus(int id, NC_ROTATOR_STATUS *status)
{
    if (!status)
    {
        return NC_ERROR_NULL_POINTER;
    }

    std::shared_ptr<Device> device;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        // Check access mode - allow if rotator is open or both are open
        if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
            it->second->accessMode != Device::AccessMode::BOTH)
        {
            return NC_ERROR_INVALID_STATE;
        }

        device = it->second;
    }
    // Global lock released here before serial operations

    // Protect device serial operations with device-level mutex
    std::lock_guard<std::mutex> deviceLock(device->deviceMutex);

    // Query position and moving status using helper
    int positionSteps = 0;
    RETURN_IF_ERROR(QueryMotorPosition(device, 2, positionSteps));
    RETURN_IF_ERROR(QueryMotorState(device, 2, device->rotator.status.moving));

    // Get corrected position in degree
    device->rotator.status.position = GetRotatorAngleFromSteps(positionSteps, device->rotator.status.stepsPerRevolution, device->rotator.config.reverseDirection);

    *status = device->rotator.status;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorFindHome(int id)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if rotator is open or both are open
    if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;

    // Use helper to find home (motorType = 2 for rotator)
    return FindMotorHome(device, 2);
}

NCAPI NC_ERROR_TYPE NCRotatorSyncPosition(int id, float angle)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if rotator is open or both are open
    if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    if (angle < 0.0f || angle >= 360.0f)
    {
        NC_ERROR("Invalid angle: %lf", angle);
        return NC_ERROR_INVALID_PARAMETER;
    }

    auto device = it->second;

    // Fetch the current position
    int currentPosition;
    RETURN_IF_ERROR(QueryMotorPosition(device, 2, currentPosition));
    RETURN_IF_ERROR(QueryMotorState(device, 2, device->rotator.status.moving));

    if (device->rotator.status.moving)
    {
        NC_ERROR("Cannot sync when moving");
        return NC_ERROR_INVALID_STATE;
    }

    // Convert degree to position
    int position = GetRotatorStepsFromAngle(currentPosition,
                                            device->rotator.status.stepsPerRevolution,
                                            angle,
                                            device->rotator.config.reverseDirection);

    NC_ERROR_TYPE rc = SetMotorSyncPosition(device, 2, position);
    if (rc == NC_SUCCESS)
    {
        device->rotator.status.position = angle;
    }
    return rc;
}

NCAPI NC_ERROR_TYPE NCRotatorMove(int id, float angle)
{
    float absAngle;

    {
        std::lock_guard<std::mutex> lock(g_globalMutex);

        auto it = g_devices.find(id);
        if (it == g_devices.end())
        {
            return NC_ERROR_INVALID_ID;
        }

        // Check access mode - allow if rotator is open or both are open
        if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
            it->second->accessMode != Device::AccessMode::BOTH)
        {
            return NC_ERROR_INVALID_STATE;
        }

        auto device = it->second;

        // Get rotator status to get steps per revolution
        NC_ROTATOR_STATUS status = device->rotator.status;
        if (status.stepsPerRevolution <= 0)
        {
            NC_ERROR("Invalid stepsPerRevolution: %d", status.stepsPerRevolution);
            return NC_ERROR_INVALID_PARAMETER;
        }

        absAngle = device->rotator.status.position + angle;

        NC_INFO("Rotator Move: current=%.2f°, relative=%.2f°, absolute=%.2f°", device->rotator.status.position, angle, absAngle);

        // Normalize to [0, 360)
        absAngle = fmod(absAngle, 360.0f);
        if (absAngle < 0.0f)
        {
            absAngle += 360.0f;
        }
    }

    return NCRotatorMoveTo(id, absAngle);
}

NCAPI NC_ERROR_TYPE NCRotatorMoveTo(int id, float angle)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if rotator is open or both are open
    if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    if (angle < 0.0f || angle >= 360.0f)
    {
        NC_ERROR("Invalid angle: %lf", angle);
        return NC_ERROR_INVALID_PARAMETER;
    }

    auto device = it->second;

    // Get rotator status to get steps per revolution
    NC_ROTATOR_STATUS status = device->rotator.status;
    if (status.stepsPerRevolution <= 0)
    {
        NC_ERROR("Invalid stepsPerRevolution: %d", status.stepsPerRevolution);
        return NC_ERROR_INVALID_PARAMETER;
    }

    // Fetch the current position
    int currentPosition;
    RETURN_IF_ERROR(QueryMotorPosition(device, 2, currentPosition));
    RETURN_IF_ERROR(QueryMotorState(device, 2, device->rotator.status.moving));

    if (device->rotator.status.moving)
    {
        NC_ERROR("Cannot sync when moving");
        return NC_ERROR_INVALID_STATE;
    }

    // Calculate absolute position from angle: angle / 360 * stepsPerRevolution
    int position = GetRotatorStepsFromAngle(currentPosition,
                                            status.stepsPerRevolution,
                                            angle,
                                            device->rotator.config.reverseDirection);

    NC_INFO("Rotator move to angle: %.2f° = position %d (%.1f steps/rev)", angle, position, (float)status.stepsPerRevolution);

    // Use helper to move to position
    NC_ERROR_TYPE rc = MoveMotorTo(device, 2, position);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    // Then start the motor
    rc = MoveMotor(device, 2, 0);
    if (rc != NC_SUCCESS)
    {
        return rc;
    }

    device->rotator.status.position = angle; // Store the original angle requested
    device->rotator.status.moving = 1;
    return NC_SUCCESS;
}

NCAPI NC_ERROR_TYPE NCRotatorStopMove(int id)
{
    std::lock_guard<std::mutex> lock(g_globalMutex);

    auto it = g_devices.find(id);
    if (it == g_devices.end())
    {
        return NC_ERROR_INVALID_ID;
    }

    // Check access mode - allow if rotator is open or both are open
    if (it->second->accessMode != Device::AccessMode::ROTATOR_ONLY &&
        it->second->accessMode != Device::AccessMode::BOTH)
    {
        return NC_ERROR_INVALID_STATE;
    }

    auto device = it->second;

    // Use helper to stop motor
    NC_ERROR_TYPE rc = StopMotor(device, 2);
    if (rc == NC_SUCCESS)
    {
        device->rotator.status.moving = 0;
    }

    return rc;
}
