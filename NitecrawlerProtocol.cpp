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

#include "NitecrawlerProtocol.h"
#include "NitecrawlerLogging.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <memory>
#include <chrono>
#include <thread>

/* Rotator steps per revolution constants */
#define WR25_STEPS_PER_REVOLUTION 374920
#define WR30_STEPS_PER_REVOLUTION 444080
#define WR35_STEPS_PER_REVOLUTION 505960

namespace Nitecrawler
{
    bool SendCommand(std::shared_ptr<Device> device, const char *command, int timeoutMs)
    {
        if (!device || !device->port || !device->port->IsOpen())
        {
            NC_DEBUG("SendCommand: device=%p, port=%p, isOpen=%d",
                     device.get(), device ? device->port.get() : nullptr,
                     device && device->port ? device->port->IsOpen() : 0);
            return false;
        }

        device->port->Drain();
        device->port->Flush();
        device->port->ClearRxBuffer();

        NC_DEBUG("SendCommand: Writing '%s'", command);
        if (!device->port->Write((const unsigned char *)command, strlen(command)))
        {
            NC_DEBUG("SendCommand: Write failed");
            return false;
        }

        return true;
    }

    int ReadResponse(std::shared_ptr<Device> device, char *response, int maxLen, int timeoutMs)
    {
        if(!device || !device->port || !device->port->IsOpen())
        {
            NC_DEBUG("ReadResponse: device=%p, port=%p, isOpen=%d",
                     device.get(), device ? device->port.get() : nullptr,
                     device && device->port ? device->port->IsOpen() : 0);
            return false;
        }

        NC_DEBUG("ReadResponse: Reading...");
        int len = device->port->Read((unsigned char *)response, maxLen, '#', 500);
        if(len == 0)
        {
            NC_DEBUG("ReadResponse: Read failed");
            return 0;
        }

        return len;
    }





    // Core response reading logic: wait for '#' terminator or timeout
    // Handles partial responses that arrive in fragments
    void ReadResponseUntilTerminator(SerialPort *port, unsigned char *response, int &bytesRead, int maxLen, int initialReadTimeoutMs)
    {
        // Initial read already done, now handle partial responses
        auto startTime = std::chrono::steady_clock::now();
        const int RESPONSE_TIMEOUT_MS = 3000; // 3 second global timeout

        while (bytesRead > 0 && bytesRead < maxLen)
        {
            // Check if we have the terminator
            if (response[bytesRead - 1] == '#')
            {
                break; // Got complete response with terminator
            }

            // Check timeout
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (elapsed > RESPONSE_TIMEOUT_MS)
            {
                break; // Timeout waiting for complete response
            }

            // Wait a bit more and try to read additional bytes
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            int moreBytes = port->Read(response + bytesRead, maxLen - bytesRead, '#', 200);

            if (moreBytes > 0)
            {
                bytesRead += moreBytes;
            }
            else
            {
                break; // No more data coming
            }
        }
    }

    // Core command sending logic: drain, delay, send, receive
    // Works with any SerialPort (device port or temporary port)
    bool SendCommandCore(SerialPort *port, const char *command, unsigned char *response, int *responseLen, int timeoutMs)
    {
        if (!port || !port->IsOpen())
        {
            return false;
        }

        // Drain any pending data and clear stale receive buffer
        port->Drain();
        port->ClearRxBuffer();

        // Send command
        int cmdLen = strlen(command);
        if (!port->Write((const unsigned char *)command, cmdLen))
        {
            return false;
        }

        // Read response
        int maxLen = responseLen ? *responseLen : 256;
        int bytesRead = port->Read(response, maxLen, '#', timeoutMs);
        if (bytesRead <= 0)
        {
            return false;
        }

        // Handle partial responses
        ReadResponseUntilTerminator(port, response, bytesRead, maxLen, timeoutMs);

        if (responseLen)
        {
            *responseLen = bytesRead;
        }

        return true;
    }

    // Send command on raw port (used for scanning)
    bool SendCommandRaw(SerialPort *port, const char *command, unsigned char *response, int *responseLen, int timeoutMs)
    {
        return SendCommandCore(port, command, response, responseLen, timeoutMs);
    }

    // Centralized command sending with drain, delays, and partial response handling
    bool SendCommand(Device *device, const char *command, unsigned char *response, int *responseLen, int timeoutMs)
    {
        std::lock_guard<std::mutex> lock(device->deviceMutex);
        return SendCommandCore(device->port.get(), command, response, responseLen, timeoutMs);
    }

    // Helper function to determine rotator steps per revolution from product model
    int GetRotatorStepsPerRevolution(const std::string &productModel)
    {
        if (productModel.find("2.5") != std::string::npos || productModel.find("25") != std::string::npos)
        {
            return WR25_STEPS_PER_REVOLUTION;
        }
        else if (productModel.find("3.0") != std::string::npos || productModel.find("30") != std::string::npos)
        {
            return WR30_STEPS_PER_REVOLUTION;
        }
        else if (productModel.find("3.5") != std::string::npos || productModel.find("35") != std::string::npos)
        {
            return WR35_STEPS_PER_REVOLUTION;
        }
        // Default to WR35 if unknown
        return WR35_STEPS_PER_REVOLUTION;
    }

    NC_ERROR_TYPE SendAndWaitForReply(std::shared_ptr<Device> device, const char* cmd, char* buffer, int maxLen, int sendTimeoutMs, int recvTimeoutMs)
    {
        // Send command
        if (!SendCommand(device, cmd, sendTimeoutMs))
        {
            NC_DEBUG("Failed to send command %s", cmd);
            return NC_ERROR_COMMUNICATION;
        }

        // Read response
        if (!ReadResponse(device, buffer, maxLen, recvTimeoutMs))
        {
            NC_DEBUG("Failed to receive response");
            return NC_ERROR_COMMUNICATION;
        }

        NC_DEBUG("Sent %s ; Received %s", cmd, buffer);

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE SendAndWaitForReplyWithRetry(std::shared_ptr<Device> device,
                                         const char *cmd,
                                         char* buffer,
                                         int maxLen,
                                         int sendTimeoutMs,
                                         int recvTimeoutMs,
                                         int maxRetries,
                                         int retryDelayMs,
                                         const char *timeoutMsg)
    {
        for (int attempt = 1; attempt <= maxRetries; ++attempt)
        {
            NC_DEBUG("SendAndWaitForReplyWithRetry: Attempt %d/%d for %s (timeout=%dms)", attempt, maxRetries, timeoutMsg, recvTimeoutMs);

            if (SendAndWaitForReply(device, cmd, buffer, maxLen, sendTimeoutMs, recvTimeoutMs) == NC_SUCCESS)
            {
                NC_DEBUG("SendAndWaitForReplyWithRetry: Success on attempt %d for %s", attempt, timeoutMsg);
                return NC_SUCCESS;
            }

            if (attempt < maxRetries)
            {
                NC_DEBUG("SendAndWaitForReplyWithRetry: Failed on attempt %d, retrying after %d ms", attempt, retryDelayMs);
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            }
        }

        NC_DEBUG("SendAndWaitForReplyWithRetry: All %d attempts failed for %s", maxRetries, timeoutMsg);
        return NC_ERROR_COMMUNICATION;
    }

    NC_ERROR_TYPE QueryProductModel(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PF#", response, 32));

        char model[8];
        if (sscanf(response, "%s#", model) == 1)
        {
            if(strncmp(model, "NACK", 4) != 0)
            {
                device->productModel = std::string(model);
            }
        }

        // If no product model queried, set default
        if (device->productModel.empty())
        {
            device->productModel = "Moonlite Nitecrawler";
        }

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryFirmwareVersion(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PV#", response, 32));

        int major = 0, minor = 0;
        if (sscanf(response, "%d.%d#", &major, &minor) == 2)
        {
            device->version.firmware = (major * 100) + minor;
            NC_INFO("Firmware version: %d.%d", major, minor);
            return NC_SUCCESS;
        }

        return NC_ERROR_COMMUNICATION;
    }

    NC_ERROR_TYPE QuerySerialNumber(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PS#", response, 32));

        int serial;
        if (sscanf(response, "%d#", &serial) == 1)
        {
            device->version.serial = serial;
            return NC_SUCCESS;
        }

        return NC_ERROR_COMMUNICATION;
    }

    static NC_ERROR_TYPE QueryDisplayBrightness(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PD#", response, 32));

        int brightness = 0;
        if (sscanf(response, "%d#", &device->config.displayBrightness) != 1)
        {
            NC_ERROR("Cound not query for display brightness");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    static NC_ERROR_TYPE QuerySleepBrightness(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PL#", response, 32));

        int brightness = 0;
        if (sscanf(response, "%d#", &device->config.sleepBrightness) != 1)
        {
            NC_ERROR("Cound not query for sleep brightness");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    static NC_ERROR_TYPE QueryVoltageOffset(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "Pv#", response, 32));

        int offset = 0;
        if (sscanf(response, "%d#", &offset) != 1)
        {
            NC_ERROR("Cound not query for voltage offset");
            return NC_ERROR_COMMUNICATION;
        }

        device->config.voltageOffset = offset * 0.1f;

        return NC_SUCCESS;
    }

    static NC_ERROR_TYPE QueryEncoderState(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "PE#", response, 32));

        if (sscanf(response, "%d#", &device->config.encoders) != 1)
        {
            NC_ERROR("Cound not query for encoder state");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    static NC_ERROR_TYPE QueryDisplayState(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        RETURN_IF_ERROR(SendAndWaitForReply(device, "C 20#", response, 32));

        if (sscanf(response, "%d#", &device->config.flipDisplay) != 1)
        {
            NC_ERROR("Cound not query for display state");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryMotorStepRate(std::shared_ptr<Device> device, int motorType)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];
        snprintf(cmd, sizeof(cmd), "%dGR#", motorType);
        RETURN_IF_ERROR(SendAndWaitForReply(device, cmd, response, 32));

        int stepRate;
        if (sscanf(response, "%d#", &stepRate) != 1)
        {
            NC_ERROR("Cound not query for step rate");
            return NC_ERROR_COMMUNICATION;
        }

        // Validate step rate (7-100)
        if (stepRate >= 7 && stepRate <= 100)
        {
            if (motorType == 1)
            {
                device->focuser.config.stepRate = stepRate;
            }
            else
            {
                device->rotator.config.stepRate = stepRate;
            }
        }
        else
        {
            if (motorType == 1)
            {
                device->focuser.config.stepRate = 7; // Default minimum
                NC_ERROR("Focuser step rate out of range (%d), using default: 7", stepRate);
            }
            else
            {
                device->rotator.config.stepRate = 7; // Default minimum
                NC_ERROR("Rotator step rate out of range (%d), using default: 7", stepRate);
            }
        }

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryMotorPosition(std::shared_ptr<Device> device, int motorType, int &position)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];

        // Query for motor position. Use the retry variant so a single transient serial glitch
        // on a polled status read does not surface as NC_ERROR_COMMUNICATION.
        snprintf(cmd, sizeof(cmd), "%dGP#", motorType);
        RETURN_IF_ERROR(SendAndWaitForReplyWithRetry(device, cmd, response, 32, 200, 500, 3, 100, "motor position"));

        if (sscanf(response, "%d#", &position) != 1)
        {
            NC_ERROR("Cound not query for position");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryMotorState(std::shared_ptr<Device> device, int motorType, int &state)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];

        // Query for motor state. Use the retry variant so a single transient serial glitch
        // on a polled status read does not surface as NC_ERROR_COMMUNICATION.
        snprintf(cmd, sizeof(cmd), "%dGM#", motorType);
        RETURN_IF_ERROR(SendAndWaitForReplyWithRetry(device, cmd, response, 32, 200, 500, 3, 100, "motor state"));

        if (sscanf(response, "%d#", &state) != 1)
        {
            NC_ERROR("Cound not query for position");
            return NC_ERROR_COMMUNICATION;
        }

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryVoltage(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];

        // Query for voltage. Use the retry variant so a single transient serial glitch
        // on a polled status read does not surface as NC_ERROR_COMMUNICATION.
        RETURN_IF_ERROR(SendAndWaitForReplyWithRetry(device, "GV#", response, 32, 200, 500, 3, 100, "voltage"));

        int voltage;
        if (sscanf(response, "%d#", &voltage) != 1)
        {
            NC_ERROR("Cound not query for voltage");
            return NC_ERROR_COMMUNICATION;
        }

        device->status.voltage = voltage * 0.1f;

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryTemperature(std::shared_ptr<Device> device)
    {
        // Default: not detected
        device->focuser.status.temperatureDetection = 0;

        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];

        // Query for temperature. Use the retry variant so a single transient serial glitch
        // on a polled status read does not surface as NC_ERROR_COMMUNICATION.
        RETURN_IF_ERROR(SendAndWaitForReplyWithRetry(device, "GT#", response, 32, 200, 500, 3, 100, "temperature"));

        int temperature;
        if (sscanf(response, "%d#", &temperature) != 1)
        {
            NC_ERROR("Cound not query for temperature");
            return NC_ERROR_COMMUNICATION;
        }

        // Convert from 0.1°C to 0.01°C
        device->focuser.status.temperatureExt = temperature * 10;
        device->focuser.status.temperatureDetection = 1;

        return NC_SUCCESS;
    }

    NC_ERROR_TYPE QueryTemperatureOffset(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        char response[32];
        char cmd[8];

        // Query for voltage
        RETURN_IF_ERROR(SendAndWaitForReply(device, "Pt#", response, 32));

        int offset;
        if (sscanf(response, "%d#", &offset) != 1)
        {
            NC_ERROR("Cound not query for temperature offset");
            return NC_ERROR_COMMUNICATION;
        }

        device->focuser.config.temperatureOffset = offset * 0.1f;

        return NC_SUCCESS;
    }

    // Helper function to initialize device properties from hardware
    NC_ERROR_TYPE InitializeDeviceProperties(std::shared_ptr<Device> device)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        if (!device->port || !device->port->IsOpen())
        {
            return NC_ERROR_COMMUNICATION;
        }

        // Only initialize once
        if (device->initialized)
        {
            return NC_ERROR_NULL_POINTER;
        }

        RETURN_IF_ERROR(QueryProductModel(device));
        RETURN_IF_ERROR(QueryFirmwareVersion(device));
        RETURN_IF_ERROR(QuerySerialNumber(device));
        RETURN_IF_ERROR(QueryDisplayBrightness(device));
        RETURN_IF_ERROR(QuerySleepBrightness(device));
        RETURN_IF_ERROR(QueryVoltageOffset(device));
        RETURN_IF_ERROR(QueryEncoderState(device));
        RETURN_IF_ERROR(QueryDisplayState(device));

        // Set rotator steps per revolution based on product model
        device->rotator.status.stepsPerRevolution = GetRotatorStepsPerRevolution(device->productModel);

        // Calculate stepsize in degrees per step
        if (device->rotator.status.stepsPerRevolution > 0)
        {
            device->rotator.status.stepSize = 360.0f / device->rotator.status.stepsPerRevolution;
        }

        NC_INFO("Rotator steps per revolution: %d (%.4f°/step) (Model: %s)",
                    device->rotator.status.stepsPerRevolution, device->rotator.status.stepSize, device->productModel.c_str());

        device->initialized = true;

        return NC_SUCCESS;
    }

    // Helper function to compute euclidian modulus
    float EuclidianModulus(double x, double y)
    {
        if (y > 0)
        {
            double r = fmodf(x, y);
            if (r < 0)
            {
                return r + y;
            }
            else
            {
                return r;
            }
        }
        else if (y < 0)
        {
            return -1 * EuclidianModulus(-1 * x, -1 * y);
        }
        else
        {
            return 0.0f;
        }
    }

    // Helper function to convert position steps to angle
    float GetRotatorAngleFromSteps(int position, int stepsPerRevolution, bool reverse)
    {
        // Apply reverse direction by flipping the position
        if (reverse)
        {
            position = -position;
        }

        float angle = (float)position / stepsPerRevolution * 360.0f;
        angle = fmod(angle, 360.0f);
        if (angle < 0.0f)
        {
            angle += 360.0f;
        }

        return angle;
    }

    // Helper function to convert position steps to angle
    int GetRotatorStepsFromAngle(int currentSteps, int stepsPerRevolution, float angle, bool reverse)
    {
        // Apply reverse direction: flip the angle
        if (reverse)
        {
            angle = -angle;
        }

        // Convert angle to target step position (0-360° range)
        float targetFraction = angle / 360.0f;
        int targetSteps = (int)(targetFraction * stepsPerRevolution);

        // Calculate shortest path from current to target
        int delta = targetSteps - currentSteps;

        // If path is longer than half a revolution, go the other way
        if (delta > stepsPerRevolution / 2)
        {
            delta -= stepsPerRevolution;
        }
        else if (delta < -stepsPerRevolution / 2)
        {
            delta += stepsPerRevolution;
        }

        // Return the final target position
        return currentSteps + delta;
    }

    // Helper function to set step rate for either focuser or rotator
    // motorType: 1 for focuser, 2 for rotator
    NC_ERROR_TYPE SetMotorStepRate(std::shared_ptr<Device> device, int motorType, int stepRate)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        // Validate step rate (7-100)
        if (stepRate < 7 || stepRate > 100)
        {
            return NC_ERROR_INVALID_PARAMETER;
        }

        // Send step rate command
        unsigned char response[256];
        int respLen = sizeof(response);
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%dSR %03d#", motorType, stepRate);

        NC_INFO("Setting step rate to %d with command: %s", stepRate, cmd);

        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send step rate command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Step rate command response: %s (len: %d)", (const char *)response, respLen);

        // Update config based on motor type
        if (motorType == 1)
        {
            device->focuser.config.stepRate = stepRate;
        }
        else if (motorType == 2)
        {
            device->rotator.config.stepRate = stepRate;
        }

        return NC_SUCCESS;
    }

    // Helper function for motor sync position (1SP or 2SP)
    NC_ERROR_TYPE SetMotorSyncPosition(std::shared_ptr<Device> device, int motorType, int position)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%dSP %d#", motorType, position);

        NC_INFO("Syncing motor position to %d with command: %s", position, cmd);

        unsigned char response[256];
        int respLen = sizeof(response);
        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send sync position command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Sync position response: %s", (const char *)response);

        return (respLen > 0 && response[0] == '#') ? NC_SUCCESS : NC_ERROR_COMMUNICATION;
    }

    // Helper function for motor move relative (1SM or 2SM)
    NC_ERROR_TYPE MoveMotor(std::shared_ptr<Device> device, int motorType, int step)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%dSM %d#", motorType, step);

        NC_INFO("Moving motor with command: %s", cmd);

        unsigned char response[256];
        int respLen = sizeof(response);
        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send move command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Move response: %s", (const char *)response);

        return (respLen > 0 && response[0] == '#') ? NC_SUCCESS : NC_ERROR_COMMUNICATION;
    }

    // Helper function for motor move to position (1SN or 2SN)
    NC_ERROR_TYPE MoveMotorTo(std::shared_ptr<Device> device, int motorType, int position)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%dSN %d#", motorType, position);

        NC_INFO("Moving motor to position %d with command: %s", position, cmd);

        unsigned char response[256];
        int respLen = sizeof(response);
        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send move to command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Move to response: %s", (const char *)response);

        return (respLen > 0 && response[0] == '#') ? NC_SUCCESS : NC_ERROR_COMMUNICATION;
    }

    // Helper function for motor stop (1SQ or 2SQ)
    NC_ERROR_TYPE StopMotor(std::shared_ptr<Device> device, int motorType)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "%dSQ#", motorType);

        NC_INFO("Stopping motor with command: %s", cmd);

        unsigned char response[256];
        int respLen = sizeof(response);
        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send stop command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Stop response: %s", (const char *)response);

        return (respLen > 0 && response[0] == '#') ? NC_SUCCESS : NC_ERROR_COMMUNICATION;
    }

    // Helper function for motor find home (SH 01 for focuser, SH 02 for rotator)
    NC_ERROR_TYPE FindMotorHome(std::shared_ptr<Device> device, int motorType)
    {
        if (!device)
        {
            return NC_ERROR_NULL_POINTER;
        }

        char cmd[32];
        snprintf(cmd, sizeof(cmd), "SH %02d#", motorType);

        NC_INFO("Finding home with command: %s", cmd);

        unsigned char response[256];
        int respLen = sizeof(response);
        if (!SendCommand(device.get(), cmd, response, &respLen, 500))
        {
            NC_ERROR("Failed to send find home command");
            return NC_ERROR_COMMUNICATION;
        }

        response[respLen] = '\0';
        NC_INFO("Find home response: %s", (const char *)response);

        return (respLen > 0 && response[respLen - 1] == '#') ? NC_SUCCESS : NC_ERROR_COMMUNICATION;
    }

} /* namespace Nitecrawler */
