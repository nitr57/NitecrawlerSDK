#include "NitecrawlerSDK.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Colors for better readability
#define COLOR_RESET     "\033[0m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_CYAN      "\033[36m"
#define COLOR_RED       "\033[31m"
#define COLOR_BLUE      "\033[34m"

int selectedDeviceId = -1;

void printMenu() {
    printf("\n%s=== Nitecrawler SDK Interactive Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n%sDevice Menu:%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  1. Scan for devices\n");
    printf("  2. Get device info\n");
    printf("  3. Get device config\n");
    printf("  4. Set device config\n");
    printf("  5. Get device status\n");
    
    printf("\n%sFocuser Menu:%s\n", COLOR_BLUE, COLOR_RESET);
    printf("  6. Open focuser\n");
    printf("  7. Close focuser\n");
    printf("  8. Get focuser config\n");
    printf("  9. Set focuser config\n");
    printf(" 10. Get focuser status\n");
    printf(" 11. Move focuser (relative steps)\n");
    printf(" 12. Move focuser to position\n");
    printf(" 13. Sync focuser position\n");
    printf(" 14. Find home (focuser)\n");
    printf(" 15. Stop focuser move\n");
    
    printf("\n%sRotator Menu:%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" 16. Open rotator\n");
    printf(" 17. Close rotator\n");
    printf(" 18. Get rotator config\n");
    printf(" 19. Set rotator config\n");
    printf(" 20. Get rotator status\n");
    printf(" 21. Move rotator (relative angle)\n");
    printf(" 22. Move rotator to position\n");
    printf(" 23. Sync rotator position\n");
    printf(" 24. Find home (rotator)\n");
    printf(" 25. Stop rotator move\n");
    
    printf("\n%sOther:%s\n", COLOR_BLUE, COLOR_RESET);
    printf(" 26. Get SDK version\n");
    printf("  0. Exit\n");
    printf("\n%sEnter choice: %s", COLOR_YELLOW, COLOR_RESET);
}

const char* errorToString(NC_ERROR_TYPE err) {
    switch(err) {
        case NC_SUCCESS: return "SUCCESS";
        case NC_ERROR_INVALID_ID: return "INVALID_ID";
        case NC_ERROR_INVALID_PARAMETER: return "INVALID_PARAMETER";
        case NC_ERROR_INVALID_STATE: return "INVALID_STATE";
        case NC_ERROR_COMMUNICATION: return "COMMUNICATION";
        case NC_ERROR_NULL_POINTER: return "NULL_POINTER";
        default: return "UNKNOWN";
    }
}

void printError(const char* operation, NC_ERROR_TYPE err) {
    printf("%s[ERROR] %s failed: %s%s\n", COLOR_RED, operation, errorToString(err), COLOR_RESET);
}

void printSuccess(const char* operation) {
    printf("%s[OK] %s%s\n", COLOR_GREEN, operation, COLOR_RESET);
}

void scanDevices() {
    printf("\n%s--- Scanning for Devices ---%s\n", COLOR_CYAN, COLOR_RESET);
    int deviceIds[32];
    int numDevices = 32;
    
    NC_ERROR_TYPE result = NCFocuserScan(&numDevices, deviceIds);
    if(result != NC_SUCCESS) {
        printError("Scan", result);
        return;
    }
    
    if(numDevices == 0) {
        printf("No devices found.\n");
        return;
    }
    
    printf("Found %d device(s)\n", numDevices);
    for(int i = 0; i < numDevices; i++) {
        printf("  [%d] Device ID: %d\n", i, deviceIds[i]);
    }
    
    printf("\nSelect device (0-%d, or -1 to skip): ", numDevices-1);
    scanf("%d", &selectedDeviceId);
    
    if(selectedDeviceId >= 0 && selectedDeviceId < numDevices) {
        selectedDeviceId = deviceIds[selectedDeviceId];
        printf("Selected device ID: %d\n", selectedDeviceId);
    } else {
        selectedDeviceId = -1;
    }
}

void getDeviceInfo() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Device Info ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    // Get product model
    char model[NC_NAME_LEN];
    NC_ERROR_TYPE result = NCGetProductModel(selectedDeviceId, model);
    if(result == NC_SUCCESS) {
        printf("Product Model: %s\n", model);
    } else {
        printError("Get Product Model", result);
    }
    
    // Get version
    NC_VERSION version;
    result = NCGetVersion(selectedDeviceId, &version);
    if(result == NC_SUCCESS) {
        printf("Firmware Version: %d.%d\n", version.firmware / 100, version.firmware % 100);
        printf("Serial: %u\n", version.serial);
    } else {
        printError("Get Version", result);
    }
}

void getDeviceConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Device Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_DEVICE_CONFIG config;
    NC_ERROR_TYPE result = NCGetConfig(selectedDeviceId, &config);
    if(result != NC_SUCCESS) {
        printError("Get Device Config", result);
        return;
    }
    
    printf("Display Brightness: %d\n", config.displayBrightness);
    printf("Sleep Brightness: %d\n", config.sleepBrightness);
    printf("Voltage Offset: %.2f\n", config.voltageOffset);
    printf("Encoders: %s\n", config.encoders ? "Enabled" : "Disabled");
    printf("Flip Display: %s\n", config.flipDisplay ? "Flipped" : "Normal");
}

void setDeviceConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Set Device Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_DEVICE_CONFIG config = {0};
    printf("Mask (0=all, 1=brightness, 2=sleep_brightness, 4=voltage_offset, 8=encoders, 16=flip_display): ");
    scanf("%u", &config.mask);
    if(config.mask == 0) config.mask = MASK_DEVICE_ALL;
    
    if(config.mask & MASK_DEVICE_BRIGHTNESS) {
        printf("Display Brightness (0-255): ");
        scanf("%d", &config.displayBrightness);
    }
    
    if(config.mask & MASK_DEVICE_SLEEP_BRIGHTNESS) {
        printf("Sleep Brightness (0-255): ");
        scanf("%d", &config.sleepBrightness);
    }
    
    if(config.mask & MASK_DEVICE_VOLTAGE_OFFSET) {
        printf("Voltage Offset (float): ");
        scanf("%f", &config.voltageOffset);
    }
    
    if(config.mask & MASK_DEVICE_ENCODERS) {
        printf("Enable Encoders (0/1): ");
        scanf("%d", &config.encoders);
    }
    
    if(config.mask & MASK_DEVICE_FLIP_DISPLAY) {
        printf("Flip Display (0/1): ");
        scanf("%d", &config.flipDisplay);
    }
    
    NC_ERROR_TYPE result = NCSetConfig(selectedDeviceId, &config);
    if(result == NC_SUCCESS) {
        printSuccess("Set Device Config");
    } else {
        printError("Set Device Config", result);
    }
}

void getDeviceStatus() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Device Status ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_DEVICE_STATUS status;
    NC_ERROR_TYPE result = NCGetStatus(selectedDeviceId, &status);
    if(result != NC_SUCCESS) {
        printError("Get Device Status", result);
        return;
    }
    
    printf("Voltage: %.2f V\n", status.voltage);
}

void focuserGetConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Focuser Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_FOCUSER_CONFIG config;
    NC_ERROR_TYPE result = NCFocuserGetConfig(selectedDeviceId, &config);
    if(result != NC_SUCCESS) {
        printError("Get Focuser Config", result);
        return;
    }
    
    printf("Max Step: %d\n", config.maxStep);
    printf("Backlash: %d\n", config.backlash);
    printf("Backlash Direction: %s\n", config.backlashDirection ? "OUT" : "IN");
    printf("Reverse Direction: %s\n", config.reverseDirection ? "Yes" : "No");
    printf("Step Rate: %d\n", config.stepRate);
    printf("Temperature Offset: %.2f°C\n", config.temperatureOffset);
}

void focuserSetConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Set Focuser Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_FOCUSER_CONFIG config = {0};
    printf("Mask (0=all, 1=max_step, 2=backlash, 4=backlash_dir, 8=reverse, 16=step_rate, 32=temp_offset): ");
    scanf("%u", &config.mask);
    if(config.mask == 0) config.mask = MASK_FOCUSER_ALL;
    
    if(config.mask & MASK_FOCUSER_MAX_STEP) {
        printf("Max Step: ");
        scanf("%d", &config.maxStep);
    }
    
    if(config.mask & MASK_FOCUSER_BACKLASH) {
        printf("Backlash: ");
        scanf("%d", &config.backlash);
    }
    
    if(config.mask & MASK_FOCUSER_BACKLASH_DIRECTION) {
        printf("Backlash Direction (0=IN, 1=OUT): ");
        scanf("%d", &config.backlashDirection);
    }
    
    if(config.mask & MASK_FOCUSER_REVERSE_DIRECTION) {
        printf("Reverse Direction (0/1): ");
        scanf("%d", &config.reverseDirection);
    }
    
    if(config.mask & MASK_FOCUSER_STEP_RATE) {
        printf("Step Rate (7-100): ");
        scanf("%d", &config.stepRate);
    }
    
    if(config.mask & MASK_FOCUSER_TEMPERATURE_OFFSET) {
        printf("Temperature Offset (-15.0 to 15.0): ");
        scanf("%f", &config.temperatureOffset);
    }
    
    NC_ERROR_TYPE result = NCFocuserSetConfig(selectedDeviceId, &config);
    if(result == NC_SUCCESS) {
        printSuccess("Set Focuser Config");
    } else {
        printError("Set Focuser Config", result);
    }
}

void focuserGetStatus() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Focuser Status ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_FOCUSER_STATUS status;
    NC_ERROR_TYPE result = NCFocuserGetStatus(selectedDeviceId, &status);
    if(result != NC_SUCCESS) {
        printError("Get Focuser Status", result);
        return;
    }
    
    printf("Temperature (External): %.2f°C\n", status.temperatureExt / 100.0f);
    printf("Temperature Probe: %s\n", status.temperatureDetection ? "Inserted" : "Not inserted");
    printf("Position: %d\n", status.position);
    printf("Moving: %s\n", status.moving ? "Yes" : "No");
    printf("Microns Per Step: %.4f\n", status.micronsPerStep);
}

void focuserMove() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Move Focuser (Relative) ---%s\n", COLOR_CYAN, COLOR_RESET);
    int steps;
    printf("Steps (negative = inward): ");
    scanf("%d", &steps);
    
    NC_ERROR_TYPE result = NCFocuserMove(selectedDeviceId, steps);
    if(result == NC_SUCCESS) {
        printSuccess("Move Focuser");
    } else {
        printError("Move Focuser", result);
    }
}

void focuserMoveTo() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Move Focuser (Absolute) ---%s\n", COLOR_CYAN, COLOR_RESET);
    int position;
    printf("Target Position: ");
    scanf("%d", &position);
    
    NC_ERROR_TYPE result = NCFocuserMoveTo(selectedDeviceId, position);
    if(result == NC_SUCCESS) {
        printSuccess("Move Focuser To");
    } else {
        printError("Move Focuser To", result);
    }
}

void focuserSync() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Sync Focuser Position ---%s\n", COLOR_CYAN, COLOR_RESET);
    int position;
    printf("Current Position: ");
    scanf("%d", &position);
    
    NC_ERROR_TYPE result = NCFocuserSyncPosition(selectedDeviceId, position);
    if(result == NC_SUCCESS) {
        printSuccess("Sync Focuser Position");
    } else {
        printError("Sync Focuser Position", result);
    }
}

void rotatorGetConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Rotator Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_ROTATOR_CONFIG config;
    NC_ERROR_TYPE result = NCRotatorGetConfig(selectedDeviceId, &config);
    if(result != NC_SUCCESS) {
        printError("Get Rotator Config", result);
        return;
    }
    
    printf("Reverse Direction: %s\n", config.reverseDirection ? "Yes" : "No");
    printf("Step Rate: %d\n", config.stepRate);
}

void rotatorSetConfig() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Set Rotator Config ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_ROTATOR_CONFIG config = {0};
    printf("Mask (0=all, 1=reverse, 2=step_rate): ");
    scanf("%u", &config.mask);
    if(config.mask == 0) config.mask = MASK_ROTATOR_ALL;
    
    if(config.mask & MASK_ROTATOR_REVERSE_DIRECTION) {
        printf("Reverse Direction (0/1): ");
        scanf("%d", &config.reverseDirection);
    }
    
    if(config.mask & MASK_ROTATOR_STEP_RATE) {
        printf("Step Rate (7-100): ");
        scanf("%d", &config.stepRate);
    }
    
    NC_ERROR_TYPE result = NCRotatorSetConfig(selectedDeviceId, &config);
    if(result == NC_SUCCESS) {
        printSuccess("Set Rotator Config");
    } else {
        printError("Set Rotator Config", result);
    }
}

void rotatorGetStatus() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Rotator Status ---%s\n", COLOR_CYAN, COLOR_RESET);
    
    NC_ROTATOR_STATUS status;
    NC_ERROR_TYPE result = NCRotatorGetStatus(selectedDeviceId, &status);
    if(result != NC_SUCCESS) {
        printError("Get Rotator Status", result);
        return;
    }
    
    printf("Position: %.2f°\n", status.position);
    printf("Moving: %s\n", status.moving ? "Yes" : "No");
    printf("Steps Per Revolution: %d\n", status.stepsPerRevolution);
    printf("Step Size: %.4f°/step\n", status.stepSize);
}

void rotatorMove() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Move Rotator (Relative) ---%s\n", COLOR_CYAN, COLOR_RESET);
    float angle;
    printf("Angle (degrees, negative = counter-clockwise): ");
    scanf("%f", &angle);
    
    NC_ERROR_TYPE result = NCRotatorMove(selectedDeviceId, angle);
    if(result == NC_SUCCESS) {
        printSuccess("Move Rotator");
    } else {
        printError("Move Rotator", result);
    }
}

void rotatorMoveTo() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Move Rotator (Absolute) ---%s\n", COLOR_CYAN, COLOR_RESET);
    float position;
    printf("Target Position (degrees): ");
    scanf("%f", &position);
    
    NC_ERROR_TYPE result = NCRotatorMoveTo(selectedDeviceId, position);
    if(result == NC_SUCCESS) {
        printSuccess("Move Rotator To");
    } else {
        printError("Move Rotator To", result);
    }
}

void rotatorSync() {
    if(selectedDeviceId < 0) {
        printf("%s[ERROR] No device selected%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s--- Sync Rotator Position ---%s\n", COLOR_CYAN, COLOR_RESET);
    float position;
    printf("Current Position (degrees): ");
    scanf("%f", &position);
    
    NC_ERROR_TYPE result = NCRotatorSyncPosition(selectedDeviceId, position);
    if(result == NC_SUCCESS) {
        printSuccess("Sync Rotator Position");
    } else {
        printError("Sync Rotator Position", result);
    }
}

void getSdkVersion() {
    printf("\n%s--- SDK Version ---%s\n", COLOR_CYAN, COLOR_RESET);
    char version[NC_VERSION_LEN];
    NC_ERROR_TYPE result = NCGetSDKVersion(version);
    if(result == NC_SUCCESS) {
        printf("SDK Version: %s\n", version);
    } else {
        printError("Get SDK Version", result);
    }
}

int main() {
    printf("\n%s╔════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Nitecrawler SDK - Interactive Test    ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Full API Configuration Tool           ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════╝%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    int choice;
    
    while(1) {
        printMenu();
        scanf("%d", &choice);
        
        // Clear input buffer
        int c;
        while((c = getchar()) != '\n' && c != EOF);
        
        switch(choice) {
            case 0:
                printf("\n%sGoodbye!%s\n", COLOR_GREEN, COLOR_RESET);
                return 0;
            case 1: scanDevices(); break;
            case 2: getDeviceInfo(); break;
            case 3: getDeviceConfig(); break;
            case 4: setDeviceConfig(); break;
            case 5: getDeviceStatus(); break;
            case 6: 
                if(selectedDeviceId >= 0 && NCFocuserOpen(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Open Focuser");
                break;
            case 7:
                if(selectedDeviceId >= 0 && NCFocuserClose(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Close Focuser");
                break;
            case 8: focuserGetConfig(); break;
            case 9: focuserSetConfig(); break;
            case 10: focuserGetStatus(); break;
            case 11: focuserMove(); break;
            case 12: focuserMoveTo(); break;
            case 13: focuserSync(); break;
            case 14:
                if(selectedDeviceId >= 0) {
                    printf("\nFinding home...\n");
                    if(NCFocuserFindHome(selectedDeviceId) == NC_SUCCESS)
                        printSuccess("Find Home");
                }
                break;
            case 15:
                if(selectedDeviceId >= 0 && NCFocuserStopMove(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Stop Focuser");
                break;
            case 16:
                if(selectedDeviceId >= 0 && NCRotatorOpen(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Open Rotator");
                break;
            case 17:
                if(selectedDeviceId >= 0 && NCRotatorClose(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Close Rotator");
                break;
            case 18: rotatorGetConfig(); break;
            case 19: rotatorSetConfig(); break;
            case 20: rotatorGetStatus(); break;
            case 21: rotatorMove(); break;
            case 22: rotatorMoveTo(); break;
            case 23: rotatorSync(); break;
            case 24:
                if(selectedDeviceId >= 0) {
                    printf("\nFinding home...\n");
                    if(NCRotatorFindHome(selectedDeviceId) == NC_SUCCESS)
                        printSuccess("Find Home");
                }
                break;
            case 25:
                if(selectedDeviceId >= 0 && NCRotatorStopMove(selectedDeviceId) == NC_SUCCESS)
                    printSuccess("Stop Rotator");
                break;
            case 26: getSdkVersion(); break;
            default:
                printf("%s[ERROR] Invalid choice%s\n", COLOR_RED, COLOR_RESET);
        }
    }
    
    return 0;
}
