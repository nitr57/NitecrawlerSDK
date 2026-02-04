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

#ifndef NITECRAWLER_PROTOCOL_H
#define NITECRAWLER_PROTOCOL_H

#include "NitecrawlerDevice.h"

namespace Nitecrawler
{
    bool SendCommand(std::shared_ptr<Device> device, const char *command, int timeoutMs = 200);
    int ReadResponse(std::shared_ptr<Device> device, char *response, int maxLen, int timeoutMs = 500);
    NC_ERROR_TYPE SendAndWaitForReply(std::shared_ptr<Device> device, const char *cmd, char *buffer, int maxLen, int sendTimeoutMs = 200, int recvTimeoutMs = 500);
    NC_ERROR_TYPE SendAndWaitForReplyWithRetry(std::shared_ptr<Device> device,
                                               const char *cmd,
                                               char *buffer,
                                               int maxLen,
                                               int sendTimeoutMs = 200,
                                               int recvTimeoutMs = 500,
                                               int maxRetries = 5,
                                               int retryDelayMs = 200,
                                               const char *timeoutMsg = "timeout");

    NC_ERROR_TYPE InitializeDeviceProperties(std::shared_ptr<Device> device);
    bool SendCommand(Device *device, const char *command, unsigned char *response, int *responseLen, int timeoutMs = 500);
    bool SendCommandRaw(SerialPort *port, const char *command, unsigned char *response, int *responseLen, int timeoutMs = 500);
    NC_ERROR_TYPE SetMotorSyncPosition(std::shared_ptr<Device> device, int motorType, int position);
    NC_ERROR_TYPE SetMotorStepRate(std::shared_ptr<Device> device, int motorType, int stepRate);
    NC_ERROR_TYPE StopMotor(std::shared_ptr<Device> device, int motorType);
    NC_ERROR_TYPE MoveMotorTo(std::shared_ptr<Device> device, int motorType, int position);
    NC_ERROR_TYPE MoveMotor(std::shared_ptr<Device> device, int motorType, int step);
    NC_ERROR_TYPE FindMotorHome(std::shared_ptr<Device> device, int motorType);
    NC_ERROR_TYPE QueryMotorPosition(std::shared_ptr<Device> device, int motorType, int &position);
    NC_ERROR_TYPE QueryMotorState(std::shared_ptr<Device> device, int motorType, int &state);
    NC_ERROR_TYPE QueryMotorStepRate(std::shared_ptr<Device> device, int motorType);
    NC_ERROR_TYPE QueryVoltage(std::shared_ptr<Device> device);
    NC_ERROR_TYPE QueryTemperature(std::shared_ptr<Device> device);
    NC_ERROR_TYPE QueryTemperatureOffset(std::shared_ptr<Device> device);
    NC_ERROR_TYPE QueryProductModel(std::shared_ptr<Device> device);
    NC_ERROR_TYPE QueryFirmwareVersion(std::shared_ptr<Device> device);
    NC_ERROR_TYPE QuerySerialNumber(std::shared_ptr<Device> device);
    int GetRotatorStepsFromAngle(int currentSteps, int stepsPerRevolution, float angle, bool reverse);
    float GetRotatorAngleFromSteps(int position, int stepsPerRevolution, bool reverse);

} /* namespace Nitecrawler */

#endif /* NITECRAWLER_PROTOCOL_H */
