#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "I2CUserClient.h"

#define IOCLASS_I2C "PPCI2CInterface"

int main (int argc, const char * argv[]) {
    CFDictionaryRef     matchingDict = NULL;
    io_iterator_t       iter = 0;
    io_service_t        service = 0;
    kern_return_t       kr;

    // Create a matching dictionary that will find PPC I2C interface user client
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
    if(kr != KERN_SUCCESS) {
        printf("Failed to find PPC I2C interface!\n");
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != 0) {
        printf("Found PPCI2CInterface user client!\n");
        IOObjectRelease(service);
    }

    return 0;
}
