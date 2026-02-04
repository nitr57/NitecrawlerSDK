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

#ifndef MOONLITE_NITECRAWLER_SDK_H
#define MOONLITE_NITECRAWLER_SDK_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WINDOWS
#define NCAPI __declspec(dllexport)
#else
#define NCAPI
#endif

#define NC_MAX_NUM      32        /* Maximum Nitecrawler devices supported by this SDK */
#define NC_VERSION_LEN  32        /* Buffer length for version strings */
#define NC_NAME_LEN     32        /* Buffer length for name strings */

#define TEMPERATURE_INVALID 0xFFFFFFFF  /* This value indicates the invalid value of ambient temperature */

    typedef enum _NC_ERROR_TYPE
    {
        NC_SUCCESS = 0,                 /* Success */
        NC_ERROR_INVALID_ID,            /* Device ID is invalid */
        NC_ERROR_INVALID_PARAMETER,     /* One or more parameters are invalid */
        NC_ERROR_INVALID_STATE,         /* Device is not in correct state for specific API call */
        NC_ERROR_COMMUNICATION,         /* Data communication error such as device has been removed from USB port */
        NC_ERROR_NULL_POINTER,          /* Caller passes null-pointer parameter which is not expected */
    } NC_ERROR_TYPE;

/*
 * Used by NCxxxSetConfig() to indicates which field wants to be set
 */
#define MASK_DEVICE_BRIGHTNESS              0x01
#define MASK_DEVICE_SLEEP_BRIGHTNESS        0x02
#define MASK_DEVICE_VOLTAGE_OFFSET          0x04
#define MASK_DEVICE_ENCODERS                0x08
#define MASK_DEVICE_FLIP_DISPLAY            0x10
#define MASK_DEVICE_ALL                     0x1F

#define MASK_FOCUSER_MAX_STEP               0x01
#define MASK_FOCUSER_BACKLASH               0x02
#define MASK_FOCUSER_BACKLASH_DIRECTION     0x04
#define MASK_FOCUSER_REVERSE_DIRECTION      0x08
#define MASK_FOCUSER_STEP_RATE              0x10
#define MASK_FOCUSER_TEMPERATURE_OFFSET     0x20
#define MASK_FOCUSER_ALL                    0x3F

#define MASK_ROTATOR_REVERSE_DIRECTION      0x01
#define MASK_ROTATOR_STEP_RATE              0x02
#define MASK_ROTATOR_ALL                    0x03

    typedef struct _NC_VERSION
    {
        unsigned int firmware;  /* Nitecrawler firmware version */
        unsigned int serial;
    } NC_VERSION;

    typedef struct _NC_DEVICE_CONFIG
    {
        unsigned int mask;          /* Used by NCSetConfig() to indicate which field wants to be set */
        int displayBrightness;
        int sleepBrightness;
        float voltageOffset;
        int encoders;               /* 0 - Disabled, 1 - Enabled */
        int flipDisplay;            /* 0 - Normal, 1 - Flipped */
    } NC_DEVICE_CONFIG;

    typedef struct _NC_DEVICE_STATUS
    {
        float voltage;              /* Current DC voltage */
    } NC_DEVICE_STATUS;

    typedef struct _NC_FOCUSER_CONFIG
    {
        unsigned int mask;              /* Used by NCFocuserSetConfig() to indicates which field wants to be set */
        int maxStep;                    /* Maximum step or position */
        int backlash;                   /* Backlash value */
        int backlashDirection;          /* Backlash direction. 0 - IN, others - OUT */
        int reverseDirection;           /* 0 - Not reverse motor moving direction, others - Reverse motor moving direction */
        int stepRate;                   /* Step rate/delay (7-100) */
        float temperatureOffset;        /* Temperature offset in degrees C (-15.0 to 15.0) */
    } NC_FOCUSER_CONFIG;

    typedef struct _NC_FOCUSER_STATUS
    {
        int temperatureExt;             /* External (ambient) temperature in 0.01 degree unit */
        int temperatureDetection;       /* 0 - ambient temperature probe is not inserted, others - ambient temperature probe is inserted */
        int position;                   /* Current motor position */
        int moving;                     /* 0 - motor is not moving, others - Motor is moving */
        float micronsPerStep;           /* Microns per step resolution (depends on focuser type) */
    } NC_FOCUSER_STATUS;

    typedef struct _NC_ROTATOR_CONFIG
    {
        unsigned int mask;              /* Used by NCRotatorSetConfig() to indicates which field wants to be set */
        int reverseDirection;           /* 0 - Not reverse motor moving direction, others - Reverse motor moving direction */
        int stepRate;                   /* Step rate/delay (7-100) */
    } NC_ROTATOR_CONFIG;

    typedef struct _NC_ROTATOR_STATUS {
        float position;                 /* Current motor position in degrees */
        int moving;                     /* 0 - motor is not moving, others - Motor is moving */
        int stepsPerRevolution;         /* Steps per full revolution (hardware dependent) */
        float stepSize;                 /* Step size in degrees per step */
    } NC_ROTATOR_STATUS;

    NCAPI NC_ERROR_TYPE NCGetProductModel(int id, char *model);
    NCAPI NC_ERROR_TYPE NCGetSDKVersion(char *version);
    NCAPI NC_ERROR_TYPE NCGetVersion(int id, NC_VERSION *version);
    NCAPI NC_ERROR_TYPE NCGetConfig(int id, NC_DEVICE_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCSetConfig(int id, NC_DEVICE_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCGetStatus(int id, NC_DEVICE_STATUS *status);

    NCAPI NC_ERROR_TYPE NCFocuserScan(int *number, int *ids);
    NCAPI NC_ERROR_TYPE NCFocuserOpen(int id);
    NCAPI NC_ERROR_TYPE NCFocuserClose(int id);
    NCAPI NC_ERROR_TYPE NCFocuserGetConfig(int id, NC_FOCUSER_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCFocuserSetConfig(int id, NC_FOCUSER_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCFocuserGetStatus(int id, NC_FOCUSER_STATUS *status);
    NCAPI NC_ERROR_TYPE NCFocuserFindHome(int id);
    NCAPI NC_ERROR_TYPE NCFocuserSyncPosition(int id, int position);
    NCAPI NC_ERROR_TYPE NCFocuserMove(int id, int step);
    NCAPI NC_ERROR_TYPE NCFocuserMoveTo(int id, int position);
    NCAPI NC_ERROR_TYPE NCFocuserStopMove(int id);

    NCAPI NC_ERROR_TYPE NCRotatorScan(int *number, int *ids);
    NCAPI NC_ERROR_TYPE NCRotatorOpen(int id);
    NCAPI NC_ERROR_TYPE NCRotatorClose(int id);
    NCAPI NC_ERROR_TYPE NCRotatorGetConfig(int id, NC_ROTATOR_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCRotatorSetConfig(int id, NC_ROTATOR_CONFIG *config);
    NCAPI NC_ERROR_TYPE NCRotatorGetStatus(int id, NC_ROTATOR_STATUS *status);
    NCAPI NC_ERROR_TYPE NCRotatorFindHome(int id);
    NCAPI NC_ERROR_TYPE NCRotatorSyncPosition(int id, float angle);
    NCAPI NC_ERROR_TYPE NCRotatorMove(int id, float angle);
    NCAPI NC_ERROR_TYPE NCRotatorMoveTo(int id, float angle);
    NCAPI NC_ERROR_TYPE NCRotatorStopMove(int id);

#ifdef __cplusplus
}
#endif

#endif /* MOONLITE_NITECRAWLER_SDK_H */
