#ifndef INTERFACE
#define INTERFACE

#if defined(_WIN32)
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

#include <stdint.h>

// EXPORT int32_t execute_bytes(uint8_t* source, long size, void(registrationCallback)(RegistrationObject_t* registration));

#endif
