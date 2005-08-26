#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "I2CUserClient.h"

#define kIONameMatchHWSensor "IOHWSensor" //IOHWSensor name match
#define kIONameMatchPPCI2C "i2c" //PPCI2C name match

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

void printServiceInfo(const void* serviceDict, CFStringEncoding encoding) {
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
    }else {
		printf("%24s %15s %7ld\n",
               CFStringGetCStringPtr(sensorLocation, encoding), 
			   CFStringGetCStringPtr(sensorType, encoding),
			   currentValue);
	}
}

/**
 * Poll I/O hardware sensor reading
 */
void pollIOHardwareSensor() {
    io_iterator_t       iter;
    io_service_t        service = 0;
    kern_return_t       kr;
    CFMutableDictionaryRef serviceDict;
    CFStringEncoding       systemEncoding = CFStringGetSystemEncoding();

	// Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault,
                                       IOServiceMatching(kIONameMatchHWSensor), &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        kr = IORegistryEntryCreateCFProperties(service, &serviceDict,
                 kCFAllocatorDefault, kNilOptions);
        if (kr == KERN_SUCCESS)
            printServiceInfo(serviceDict, systemEncoding);
        CFRelease(serviceDict);
        IOObjectRelease(service);
    }
	
	IOObjectRelease(iter);
}

int main (int argc, const char * argv[]) {
    io_iterator_t       iter;
    io_service_t        service = 0;
    kern_return_t       kr;
    CFMutableDictionaryRef serviceDict;
    CFStringEncoding       systemEncoding = CFStringGetSystemEncoding();

	// Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault,
                                       IOServiceNameMatching(kIONameMatchPPCI2C), &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        printf("Found device "kIONameMatchPPCI2C" !\n\n");
		IOObjectRelease(service);
    }
	
	IOObjectRelease(iter);

    return 0;
}
