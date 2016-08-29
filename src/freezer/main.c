#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOI2C.h"

#define kIOHWSensor "IOHWSensor" //IOHWSensor name match
#define kIONameMatchPPCI2C "i2c" //PPCI2C name match
#define kIOClassValuePPCI2CInterface "PPCI2CInterface"
#define kIOProviderClassValueIOPlatformDevice "IOPlatformDevice"

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

/**
 * @brief pollADT746XChipViaI2C
 */
int pollADT746XChipViaI2C() {
    io_iterator_t           iter;
    io_service_t            service = 0;
    kern_return_t           kr;
    CFMutableDictionaryRef  matchingDictionary;

    // Create PPC I2C interface matching dictionary
    matchingDictionary = IOServiceNameMatching(kIONameMatchPPCI2C);
    // Add a key value pair: (IOClass, PPCI2CInterface) to further filter
    CFDictionaryAddValue(matchingDictionary, CFSTR(kIOClassKey),
                         CFSTR(kIOClassValuePPCI2CInterface));
    // Add a key value pair: (IOProviderClass, IOPlatformDevice) to further filter
    CFDictionaryAddValue(matchingDictionary, CFSTR(kIOProviderClassKey),
                         CFSTR(kIOProviderClassValueIOPlatformDevice));

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
            printf("Found I2C controller with class name %s matched!\n", className);
        else
            printf("Found I2C controller with unknown class name!\n");

        if(0 == strcmp(className, kIOProviderClassValueIOPlatformDevice))
            printf("Found a match to create a connection!\n");

        IOObjectRelease(service);
    }

    IOObjectRelease(iter);

    return 0;
}

/**
 * @brief pollIOI2C use IOI2C helper function
 * @return
 */
int pollIOI2C() {
    CFArrayRef i2cArrayRef = (CFArrayRef) findI2CInterfaces();
    int i;
    for(i = 0; i < CFArrayGetCount(i2cArrayRef); i++) {
        CFStringRef sName = (CFStringRef)CFArrayGetValueAtIndex( i2cArrayRef, i );
        printf("found I2C interface: %s\n",
               CFStringGetCStringPtr( sName , kCFStringEncodingMacRoman ));
    }
    return 0;
}

int main (int argc, const char * argv[]) {
    printf("Poll from IOHWSensor:\n");
    pollIOHWSensor();
    printf("\nPoll from I2C bus:\n");
    pollADT746XChipViaI2C();
    printf("\nPoll from IOI2C helper:\n");
    pollIOI2C();
    return 0;
}
