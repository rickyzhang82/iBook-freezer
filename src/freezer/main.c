#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "I2CUserClient.h"

#define kDriverClassName "IOHWSensor"

int main (int argc, const char * argv[]) {
    io_iterator_t       iter;
    io_service_t        service = 0;
    kern_return_t       kr;

	// Create an iterator for all IO Registry objects that match the dictionary
    kr =  IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kDriverClassName), &iter);
    if(kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices returned 0x%08x\n\n", kr);
        return -1;
    }

    // Iterate over all matching objects
    while((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        printf("Found a devicec class "kDriverClassName"\n\n");
        IOObjectRelease(service);
    }
	
	IOObjectRelease(iter);

    return 0;
}
