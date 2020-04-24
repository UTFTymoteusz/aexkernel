#pragma once

#include "aex/rcparray.h"

// A bus contains drivers and devices
// A driver binds to a device
// A class registers a driver to the upper layers,
//   utilizing some kind of an interface exposed
//   by the driver. However, a driver can do that
//   work on it's own accord.

enum dresource_type {
    DRT_Memory = 0,
    DRT_IO     = 1,
    DRT_IRQ    = 2,
    DRT_Other  = 3,
};

struct dattribute {
    char name[16];
    uint64_t value;
};
typedef struct dattribute dattribute_t;

struct dresource {
    enum dresource_type type;

    union {
        uint64_t start;
        uint64_t value;
    };
    uint64_t end;
};
typedef struct dresource dresource_t;

struct device {
    char name[32];
    char class[32];

    void* class_data;

    rcparray(dattribute_t) attributes;
    rcparray(dresource_t)  resources;

    void* device_private;
    void* driver_data;
};
typedef struct device device_t;

struct driver {
    char name[32];

    int (*check)(device_t* device);
    int (*bind) (device_t* device);
};
typedef struct driver driver_t;

struct class {
    char name[32];

    int (*bind)(device_t* device);
};
typedef struct class class_t;

struct bus {
    char name[32];
    rcparray(device_t)  devices;
    rcparray(driver_t*) drivers;
};
typedef struct bus bus_t;

/*
 * Adds a new bus to the device tree. Return 0 on success, 1 if the bus already
 * exists and -1 otherwise.
 */
int dev_add_bus(char* name);

/*
 * Adds a device to the specified bus. This function copies the specified 
 * device struct so please do your changes before adding the device.
 */
int dev_add(char* bus, device_t* device);

/*
 * Sets an attribute of a device. This must be done before adding the device.
 */
int dev_set_attribute(device_t* device, char* name, uint64_t val);

/*
 * Gets an attribute of a device.
 */
long dev_get_attribute(device_t* device, char* name);

/*
 * Assigns a resource to a device. This must be done before adding the device.
 */
int dev_assign_resource(device_t* device, dresource_t* ress);

/*
 * Gets an assigned resource of the device.
 */
dresource_t* dev_get_resource(device_t* device, int id);

/*
 * Registers a driver to the specified bus.
 */
int dev_register_driver(char* bus, driver_t* driver);

/*
 * Registers a class.
 */
int dev_register_class(class_t* class);

/*
 * Sets the class of a device and binds it if a class is found.
 */
int dev_set_class(device_t* device, char* class);

void dev_debug();