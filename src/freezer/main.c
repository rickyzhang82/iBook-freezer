#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOI2C.h"
#include "IOI2CDefs.h"

#define kIOHWSensor "IOHWSensor" //IOHWSensor name match
#define kIONamMatchADT7467 "adt7467"

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

    CFMutableDictionaryRef dictRef;
    kr = IORegistryEntryCreateCFProperties(service, &dictRef, kCFAllocatorDefault, kNilOptions);
    if(kr == KERN_SUCCESS) {
            CFShow(dictRef);
            CFRelease(dictRef);
    }

    io_string_t device_path, service_path;
    kr = IORegistryEntryGetPath(service, kIODeviceTreePlane, device_path);
    if(kr == KERN_SUCCESS)
        printf("\tFound IORegistryEntrywith path %s\n", device_path);

    kr = IORegistryEntryGetPath(service, kIOServicePlane, service_path);
    if(kr == KERN_SUCCESS)
        printf("\tFound IORegistryEntry with path %s\n", service_path);

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

/**
 * @brief pollFromI2C
 */
int pollFromI2C() {
    io_iterator_t           iter;
    io_service_t            service = 0;
    kern_return_t           kr;
    CFMutableDictionaryRef  matchingDictionary;

    // Create PPC I2C interface matching dictionary
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
        printIORegistryEntryInfo(service);
        io_service_t            childService = 0;
        kr = IORegistryEntryGetChildEntry(service, kIOServicePlane, &childService);
        if(kr != KERN_SUCCESS) {
            fprintf(stderr, "IORegistryEntryGetChildEntry returned 0x%08x\n\n", kr);
            return -1;
        }
        printf("Begin Child Service ---------------------------------\n");
        printIORegistryEntryInfo(childService);
        printf("End Child Service   ---------------------------------\n");
        testUserClient(childService);
        IOObjectRelease(childService);
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
