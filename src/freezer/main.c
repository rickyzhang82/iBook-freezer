#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOI2C.h"
#include "IOI2CDefs.h"
#include "freezer.h"

#define kIOHWSensor "IOHWSensor" //IOHWSensor name match
#define kIONamMatchADT7467 "adt7467" //ADT7467 chip name match
#define kIOServicePathToIOI2CADT746x \
"IOService:/MacRISC2PE/uni-n@f8000000/AppleUniN/i2c@%x/IOI2CControllerPPC/i2c-bus@%x/IOI2CBus/fan@%x"
#define kIOI2CADT746xClassName "IOI2CADT746x"
#define kIOI2CControllerPPCClassname "IOI2CControllerPPC"

#define kMethodIndexLockI2CBus              0
#define kMethodIndexUnlockI2CBus            1
#define kMethodIndexReadI2CBus              2
#define kMethodIndexWriteI2CBus             3
#define kMethodIndexReadModifyWriteI2CBus   4

#define kNumVariable 3
#define SHOULD_PRINT_DICT 0

#define kIOPPluginCurrentValueKey "current-value" // current measured value
#define kIOPPluginLocationKey     "location"      // readable description
#define kIOPPluginTypeKey         "type"          // sensor/control type
#define kIOPPluginTypeTempSensor  "temperature"   // desired type value
#define kIOPPluginTypeVoltSensor  "voltage"   // desired type value
#define kIOPPluginTypeFanSpeedSensor  "fanspeed"   // desired type value

//macro to convert sensor voltage
#define SENSOR_VOLT_FMT(x) ((x) / (double)(2 << 16))

// macro to convert sensor temperature format (16.16) to integer (Celsius)
#define SENSOR_TEMP_FMT_C(x) (double)((x) >> 16)

// macro to convert sensor temperature format (16.16) to integer (Fahrenheit)
#define SENSOR_TEMP_FMT_F(x) \
    (double)((((double)((x) >> 16) * (double)9) / (double)5) + (double)32)

//#define DEBUG 1

/**
 * @brief printSensorsInfo
 * @param serviceDict
 * @param encoding
 */
void printSensorsInfo(const void* serviceDict, CFStringEncoding encoding) {
    SInt32      currentValue;
    CFNumberRef sensorValue;
    CFStringRef sensorType, sensorLocation;

    if (!CFDictionaryGetValueIfPresent((CFDictionaryRef)serviceDict,
                                       CFSTR(kIOPPluginTypeKey),
                                       (void *)&sensorType))
        return;

    sensorLocation = CFDictionaryGetValue((CFDictionaryRef)serviceDict,
                                          CFSTR(kIOPPluginLocationKey));

    sensorValue = CFDictionaryGetValue((CFDictionaryRef)serviceDict,
                                       CFSTR(kIOPPluginCurrentValueKey));

    (void)CFNumberGetValue(sensorValue, kCFNumberSInt32Type,
                           (void *)&currentValue);
                           
    if (CFStringCompare(sensorType, CFSTR(kIOPPluginTypeTempSensor), 0) ==
            kCFCompareEqualTo) {
        printf("%24s %15s %7.1f C %9.1f F\n",
               // see documentation for CFStringGetCStringPtr() caveat
               CFStringGetCStringPtr(sensorLocation, encoding),
               CFStringGetCStringPtr(sensorType, encoding),
               SENSOR_TEMP_FMT_C(currentValue),
               SENSOR_TEMP_FMT_F(currentValue));
    } else if(CFStringCompare(sensorType, CFSTR(kIOPPluginTypeVoltSensor), 0) ==
              kCFCompareEqualTo){
        printf("%24s %15s %7.3f V\n",
               CFStringGetCStringPtr(sensorLocation, encoding), 
               CFStringGetCStringPtr(sensorType, encoding),
               SENSOR_VOLT_FMT(currentValue));
    } else {
        printf("%24s %15s %7ld\n",
               CFStringGetCStringPtr(sensorLocation, encoding), 
               CFStringGetCStringPtr(sensorType, encoding),
               currentValue);
    }
}

void printIORegistryEntryInfo(io_service_t service) {
    kern_return_t           kr;
    printf("\n\nBegin printIORegistryEntryInfo--------------------\n\n");
    io_name_t className;
    kr = IOObjectGetClass(service, className);
    if(kr == KERN_SUCCESS)
        printf("Found IORegistryEntry with class name %s\n", className);

#if SHOULD_PRINT_DICT == 1
    CFMutableDictionaryRef dictRef;
    kr = IORegistryEntryCreateCFProperties(service, &dictRef, kCFAllocatorDefault, kNilOptions);
    if(kr == KERN_SUCCESS) {
            CFShow(dictRef);
            CFRelease(dictRef);
    }
#endif

    io_string_t device_path, service_path;
    kr = IORegistryEntryGetPath(service, kIODeviceTreePlane, device_path);
    if(kr == KERN_SUCCESS)
        printf("Found IORegistryEntrywith path %s\n", device_path);

    kr = IORegistryEntryGetPath(service, kIOServicePlane, service_path);
    if(kr == KERN_SUCCESS)
        printf("Found IORegistryEntry with path %s\n", service_path);
    int chipNum, i2cBusNum, i2cContrlNum;
    if(kNumVariable ==
            sscanf(service_path, kIOServicePathToIOI2CADT746x, &chipNum, &i2cBusNum, &i2cContrlNum)) {
        printf("found @ 0x%x\n", chipNum);
        printf(" - Bus I2C @ 0x%x\n", i2cBusNum);
        printf(" - Controller @ 0x%x\n", i2cContrlNum);
    }

    printf("\n\nEnd printIORegistryEntryInfo--------------------\n\n");
}

/**
 * @brief pollIOHWSensor Poll I/O hardware sensor reading
 */
int pollIOHWSensor() {
    io_iterator_t           iter;
    io_service_t            service = 0;
    kern_return_t           kr;
    CFMutableDictionaryRef  serviceDict;
    CFStringEncoding        systemEncoding = CFStringGetSystemEncoding();

    // Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault,
                                       IOServiceMatching(kIOHWSensor), &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        kr = IORegistryEntryCreateCFProperties(service, &serviceDict,
                 kCFAllocatorDefault, kNilOptions);
        if (kr == KERN_SUCCESS)
            printSensorsInfo(serviceDict, systemEncoding);
        CFRelease(serviceDict);
        IOObjectRelease(service);
    }
    
    IOObjectRelease(iter);
    return 0;
}

void testUserClient(io_service_t service) {
    kern_return_t           kr;
    io_connect_t            dataPort;
    kr = IOServiceOpen(service, mach_task_self(), kIOI2CUserClientType, &dataPort);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceOpen returned 0x%08x\n", kr);
        return;
    } else {
        printf("IOServiceOpen was successful.\n");
    }

    kr = IOServiceClose(dataPort);
    if (kr == KERN_SUCCESS) {
        printf("IOServiceClose was successful.\n\n");
    }
    else {
        fprintf(stderr, "IOServiceClose returned 0x%08x\n\n", kr);
    }
}

int isFoundIOI2CADT746x(io_service_t parentService) {
    kern_return_t           kr;
    io_service_t            childService = 0;
    kr = IORegistryEntryGetChildEntry(parentService, kIOServicePlane, &childService);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IORegistryEntryGetChildEntry returned 0x%08x\n\n", kr);
        return -1;
    }

    io_name_t className;
    kr = IOObjectGetClass(childService, className);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOObjectGetClass returned 0x%08x\n\n", kr);
        IOObjectRelease(childService);
        return -1;
    }

    //found IOI2CADT746x class
    if(0 != strcmp(className, kIOI2CADT746xClassName)) {
        IOObjectRelease(childService);
        return -1;
    }
    
    D(printf("Found class %s\n", className));

    io_string_t service_path;
    kr = IORegistryEntryGetPath(childService, kIOServicePlane, service_path);
    if(kr == KERN_SUCCESS)
        D(printf("Found IORegistryEntry with path %s\n", service_path));
    int chipNum, i2cBusNum, i2cContrlNum;
    if(kNumVariable !=
            sscanf(service_path, kIOServicePathToIOI2CADT746x, &chipNum, &i2cBusNum, &i2cContrlNum)) {
        IOObjectRelease(childService);
        return -1;
    }

    printf("found @ 0x%x\n", chipNum);
    printf(" - Bus I2C @ 0x%x\n", i2cBusNum);
    printf(" - Controller @ 0x%x\n", i2cContrlNum);
    IOObjectRelease(childService);
    return i2cBusNum;
}

io_service_t matchI2CControllerService(io_service_t service) {
    kern_return_t           kr;
    io_service_t childService = service;
    io_service_t parentService = 0;
    io_name_t className;

    do {
        kr = IORegistryEntryGetParentEntry(childService, kIOServicePlane, &parentService);
        if(kr != KERN_SUCCESS) {
            fprintf(stderr, "IORegistryEntryGetParentEntry returned 0x%08x\n\n", kr);
            return 0;
        }

        kr = IOObjectGetClass(parentService, className);
        if(kr != KERN_SUCCESS) {
            fprintf(stderr, "IOObjectGetClass returned 0x%08x\n\n", kr);
            return 0;
        }

        D(printf("Found parent service with class name %s\n", className));

        if(childService != service)
            IOObjectRelease(childService);

        childService = parentService;

    } while(0 != strcmp(className, kIOI2CControllerPPCClassname));

    return parentService;
}

kern_return_t i2cControllerOpen(io_service_t service, io_connect_t *connect) {
    return IOServiceOpen(service, mach_task_self(), 0, connect);
}

kern_return_t i2cControllerClose(io_connect_t connect) {
    return IOServiceClose(connect);
}

kern_return_t i2cControllerLock(io_connect_t        connect,
                                UInt32              bus,
                                UInt32              *clientKeyRef) {
    return IOConnectMethodScalarIScalarO(connect,
                                         kMethodIndexLockI2CBus,
                                         1,
                                         1,
                                         bus,
                                         clientKeyRef);
}

kern_return_t i2cControllerUnlock(io_connect_t      connect,
                                  UInt32            clientKey) {
    return IOConnectMethodScalarIScalarO(connect,
                                         kMethodIndexUnlockI2CBus,
                                         1,
                                         0,
                                         clientKey);
}

kern_return_t i2cControllerRead(io_connect_t        connect,
                                I2CUserReadInput*   input,
                                IOByteCount*        structureOutputSize,
                                I2CUserReadOutput*  output) {
    return IOConnectMethodStructureIStructureO(connect,
                                         kMethodIndexReadI2CBus,
                                         sizeof(*input),
                                         structureOutputSize,
                                         input,
                                         output);
}

kern_return_t i2cControllerWrite(io_connect_t        connect,
                                I2CUserWriteInput*   input,
                                IOByteCount*        structureOutputSize,
                                I2CUserWriteOutput*  output) {
    return IOConnectMethodStructureIStructureO(connect,
                                         kMethodIndexWriteI2CBus,
                                         sizeof(*input),
                                         structureOutputSize,
                                         input,
                                         output);
}

int readFromI2CController(io_service_t service) {
    kern_return_t           kr;
    io_connect_t            connect;
    UInt32                  clientKey;
    io_service_t            i2cControllerService = 0;
    //get I2C bus number
    int busNum = isFoundIOI2CADT746x(service);

    if(-1 == busNum){
        fprintf(stderr, "Failed to find i2c bus\n\n");
        return -1;
    }

    //match I2CControllerPPC
    i2cControllerService = matchI2CControllerService(service);

    if(0 == i2cControllerService){
        fprintf(stderr, "Failed to find I2CControllerPPC\n\n");
        return -1;
    }

    //open user client of I2CControllerPPC
    kr = i2cControllerOpen(i2cControllerService, &connect);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceOpen returned 0x%08x\n", kr);
        goto ERROR_RELEASE_SERVICE;
    } else {
        D(printf("IOServiceOpen was successful.\n"));
    }

    //lock I2C Bus
    kr = i2cControllerLock(connect, busNum, &clientKey);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "i2cControllerLock returned 0x%08x\n", kr);
        goto ERROR_RELEASE_SERVICE;
    } else {
        D(printf("i2cControllerLock was successful.\n"));
    }

    //unlock I2C Bus
    kr = i2cControllerUnlock(connect, clientKey);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "i2cControllerLock returned 0x%08x\n", kr);
        goto ERROR_RELEASE_SERVICE;
    } else {
        D(printf("i2cControllerLock was successful.\n"));
    }

    //close user client of I2CControllerPPC
    kr = i2cControllerClose(connect);
    if (kr == KERN_SUCCESS) {
        D(printf("IOServiceClose was successful.\n\n"));
    }
    else {
        fprintf(stderr, "IOServiceClose returned 0x%08x\n\n", kr);
        goto ERROR_RELEASE_SERVICE;
    }

    IOObjectRelease(i2cControllerService);
    return 0;

ERROR_RELEASE_SERVICE:
    IOObjectRelease(i2cControllerService);
    return -1;
}

/**
 * @brief pollFromI2C
 */
int pollFromI2C() {
    io_iterator_t           iter;
    io_service_t            service = 0;
    kern_return_t           kr;
    CFMutableDictionaryRef  matchingDictionary;

    // Create adt7467 name matching dictionary
    matchingDictionary = IOServiceNameMatching(kIONamMatchADT7467);

    // Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault,
                                       matchingDictionary, &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        readFromI2CController(service);
        IOObjectRelease(service);
    }

    IOObjectRelease(iter);

    return 0;
}

int main (int argc, const char * argv[]) {
    printf("Poll from IOHWSensor:\n");
    pollIOHWSensor();
    printf("\nPoll from I2C bus:\n");
    pollFromI2C();
    return 0;
}
