#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOI2C.h"
#include "IOI2CDefs.h"

#define kIOHWSensor "IOHWSensor" //IOHWSensor name match
#define kIONameMatchI2C "i2c" //PPCI2C name match
#define kIONameMatchI2C_BUS "i2c-bus" //PPCI2C name match
#define kIONameMatchIOI2CPPC "uni-n-i2c-control"
#define kIOClassValuePPCI2CInterface "PPCI2CInterface"
#define kIOUserClientClassValueI2CUserClient "I2CUserClient"

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
 * @brief pollADT746XChipViaI2C
 */
int pollADT746XChipViaI2C() {
    io_iterator_t           iter;
    io_service_t            service = 0;
    kern_return_t           kr;
    CFMutableDictionaryRef  matchingDictionary;

    // Create PPC I2C interface matching dictionary
    matchingDictionary = IOServiceNameMatching(kIONameMatchIOI2CPPC);

    // Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault,
                                       matchingDictionary, &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        io_name_t className;
        kr = IOObjectGetClass(service, className);
        if(kr == KERN_SUCCESS)
            printf("Found I2C controller with class name %s\n", className);

        io_string_t device_path, service_path;
        kr = IORegistryEntryGetPath(service, kIODeviceTreePlane, device_path);
        if(kr == KERN_SUCCESS)
            printf("\tFound I2C controller with path %s\n", device_path);

        kr = IORegistryEntryGetPath(service, kIOServicePlane, service_path);
        if(kr == KERN_SUCCESS)
            printf("\tFound I2C controller with path %s\n", service_path);

        testUserClient(service);

        IOObjectRelease(service);
    }

    IOObjectRelease(iter);

    return 0;
}

static void printKeys (const void* key, const void* value, void* context) {
  CFShowStr(key);
  CFShowStr(value);
}

/**
 * @brief pollIOI2C use IOI2C helper function
 * @return
 */
int pollIOI2C() {
    CFArrayRef i2cArrayRef = (CFArrayRef) findPPCI2CInterfaces();
    int i;
    for(i = 0; i < CFArrayGetCount(i2cArrayRef); i++) {
        CFDictionaryRef dictRef = (CFDictionaryRef)CFArrayGetValueAtIndex( i2cArrayRef, i );
        CFDictionaryApplyFunction(dictRef, printKeys, NULL);
    }
    return 0;
}

int main (int argc, const char * argv[]) {
    printf("Poll from IOHWSensor:\n");
    pollIOHWSensor();
    printf("\nPoll from I2C bus:\n");
    pollADT746XChipViaI2C();
    return 0;
}
