# Moonlite Nitecrawler SDK

A complete implementation of the Moonlite Nitecrawler focuser SDK for Windows and Linux.

## Features

- Full USB focuser device support via serial communication
- Cross-platform implementation (Windows and Linux)
- Thread-safe device management
- Device enumeration and scanning
- Position control and monitoring
- Configuration management (backlash, max steps, direction)
- Temperature monitoring support
- Factory reset capability

## Architecture

### Components

- **NitecrawlerSDK.h**: Public API header file defining all SDK interfaces
- **NitecrawlerSDK.cpp**: Full C++ implementation with platform-specific serial I/O
- **test_sdk.cpp**: Comprehensive test suite demonstrating SDK usage
- **CMakeLists.txt**: Build configuration for Windows and Linux

### Serial Port Abstraction

The SDK uses an abstraction layer for serial communication:
- `WindowsSerialPort`: Windows COM port implementation
- `LinuxSerialPort`: Linux tty/USB implementation

Both implementations provide:
- Non-blocking I/O with timeout support
- Automatic 9600 baud configuration
- 8N1 serial format (8 data bits, no parity, 1 stop bit)

### Device Management

Internal device tracking with:
- Thread-safe access using std::mutex
- Device state persistence (position, configuration, status)
- Automatic device ID management

## Building

### Prerequisites

- CMake 3.10 or higher
- C++14 compatible compiler
- On Linux: Standard development tools (gcc/clang)
- On Windows: untested

### Build Steps

#### Linux
```bash
mkdir build
cd build
cmake ..
make
```

### Output

After building:
- Shared library: `build/lib/libNitecrawlerSDK.so` (Linux) or `build/bin/NitecrawlerSDK.dll` (Windows)
- Test executable: `build/bin/nitecrawler_test`

## API Usage

### Scanning for Devices

```c
int deviceCount = 32;
int deviceIds[32];
MLNCFocuserScan(&deviceCount, deviceIds);
```

### Opening and Closing Devices

```c
MLNCFocuserOpen(deviceId);
// ... use device ...
MLNCFocuserClose(deviceId);
```

### Getting Device Information

```c
char model[MLNC_FOCUSER_NAME_LEN];
char sn[MLNC_FOCUSER_NAME_LEN];
MLNC_VERSION version;

MLNCFocuserGetProductModel(id, model);
MLNCFocuserGetSerialNumber(id, sn);
MLNCFocuserGetVersion(id, &version);
```

### Movement Control

```c
// Move to absolute position
MLNCFocuserMoveTo(id, 5000);

// Move by relative steps
MLNCFocuserMove(id, 100);

// Stop movement
MLNCFocuserStopMove(id);
```

### Configuration

```c
MLNC_FOCUSER_CONFIG config;
MLNCFocuserGetConfig(id, &config);

// Modify configuration
config.mask = MASK_BACKLASH | MASK_MAX_STEP;
config.backlash = 10;
config.maxStep = 10000;

MLNCFocuserSetConfig(id, &config);
```

### Status Monitoring

```c
MLNC_FOCUSER_STATUS status;
MLNCFocuserGetStatus(id, &status);

printf("Position: %d\n", status.position);
printf("Moving: %s\n", status.moving ? "Yes" : "No");
printf("Temperature: %.2f°C\n", status.temperatureExt / 100.0f);
```

## Error Handling

All API functions return an `MLNC_ERROR_TYPE` value:
- `MLNC_SUCCESS`: Operation completed successfully
- `MLNC_ERROR_INVALID_ID`: Device ID not found
- `MLNC_ERROR_INVALID_PARAMETER`: Invalid parameter provided
- `MLNC_ERROR_COMMUNICATION`: Serial communication error
- `MLNC_ERROR_TIMEOUT`: Operation timed out
- `MLNC_ERROR_NULL_POINTER`: NULL pointer parameter provided
- Other error codes for various failure conditions

## Running Tests

```bash
./build/bin/nitecrawler_test
```

The test program will:
1. Query SDK version
2. Scan for connected devices
3. For each device, test all API operations
4. Validate device responses
5. Report results

## Platform-Specific Notes

### Linux
- Requires proper udev rules for USB device access
- May need to add user to `dialout` group: `sudo usermod -a -G dialout $USER`

## Support

For issues or questions regarding:
- Moonlite Nitecrawler hardware: https://www.focuser.com/
- NINA software: https://nighttime-imaging.eu/
