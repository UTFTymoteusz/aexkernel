#include "aex/rcparray.h"
#include "aex/string.h"

#include "aex/dev/tree.h"

rcparray(bus_t)    buses;
rcparray(class_t*) classes;

int dev_add_bus(char* name) {
    int id = -1;
    bus_t* bus = rcparray_find(buses, bus_t, 
                        strcmp(elem->name, name) == 0, &id);

    if (bus != NULL) {
        rcparray_unref(buses, id);
        printk(PRINTK_WARN "tree: Tried to add already-existing bus '%s'\n", name);
        return 1;
    }
    bus_t nbus = {0};
    snprintf(nbus.name, sizeof(nbus.name), "%s", name);

    rcparray_add(buses, nbus);
    printk("tree: Added bus '%s'\n", name);
    return 0;
}

static void _try_bind_device(bus_t* bus, device_t* device) {
    int did;
    driver_t* drv;

    rcparray_foreach(bus->drivers, did) {
        drv = rcparray_get(bus->drivers, did);
        if (drv == NULL)
            continue;

        int ret = drv->check(device);
        if (ret == 0) {
            drv->bind(device);
            continue;
        }
        rcparray_unref(bus->devices, did);
    }
}

int dev_add(char* bus, device_t* device) {
    int id = -1;
    bus_t* busc = rcparray_find(buses, bus_t, 
                        strcmp(elem->name, bus) == 0, &id);

    if (busc == NULL)
        return -1;

    device_t* check = rcparray_find(busc->devices, device_t,
                            strcmp(elem->name, device->name) == 0, &id);

    if (check != NULL)
        return -1;

    id = rcparray_add(busc->devices, *device);
    device = rcparray_get(busc->devices, id);

    printk("tree: Added device '%s/%s'\n", bus, device->name);
    
    _try_bind_device(busc, device);
    rcparray_unref(busc->devices, id);
    return 0;
}

int dev_set_attribute(device_t* device, char* name, uint64_t val) {
    int id = -1;
    dattribute_t* attr = rcparray_find(device->attributes, dattribute_t,
                                    strcmp(elem->name, name) == 0, &id);

    if (attr != NULL) {
        attr->value = val;
        rcparray_unref(device->attributes, id);

        return 1;
    }

    dattribute_t new_attr = {0};
    snprintf(new_attr.name, sizeof(new_attr.name), "%s", name);
    new_attr.value = val;

    rcparray_add(device->attributes, new_attr);
    return 0;
}

long dev_get_attribute(device_t* device, char* name) {
    int id = -1;
    dattribute_t* attr = rcparray_find(device->attributes, dattribute_t,
                                    strcmp(elem->name, name) == 0, &id);

    if (attr == NULL)
        return 0;

    return attr->value;
}

dresource_t* dev_get_resource(device_t* device, int id) {
    dresource_t* dress = rcparray_get(device->resources, id);
    rcparray_unref(device->resources, id);

    return dress;
}

int dev_assign_resource(device_t* device, dresource_t* ress) {
    rcparray_add(device->resources, *ress);
    return 0;
}

static void _try_bind_driver(bus_t* bus, driver_t* driver) {
    int did;
    device_t* dev;

    rcparray_foreach(bus->devices, did) {
        dev = rcparray_get(bus->devices, did);
        if (dev == NULL)
            continue;

        int ret = driver->check(dev);
        if (ret == 0) {
            driver->bind(dev);
            continue;
        }
        rcparray_unref(bus->devices, did);
    }
} 

int dev_register_driver(char* bus, driver_t* driver) {
    int id  = -1;
    int bid = -1;
    bus_t* busc = rcparray_find(buses, bus_t, 
                        strcmp(elem->name, bus) == 0, &bid);

    if (busc == NULL)
        return -1;

    if (rcparray_find(busc->drivers, driver_t*, 
                strcmp((*elem)->name, driver->name) == 0, &id) != NULL) {
        
        rcparray_unref(busc->drivers, id);
        rcparray_unref(buses, bid);
        return -1;
    }

    id = rcparray_add(busc->drivers, driver);
    rcparray_get(busc->drivers, id);
    printk("tree: Registered driver '%s/%s'\n", bus, driver->name);
    
    _try_bind_driver(busc, driver);
    rcparray_unref(busc->devices, id);
    rcparray_unref(buses, bid);
    
    return 0;
}

int dev_register_class(class_t* class) {
    int id = -1;

    if (rcparray_find(classes, class_t*, 
                        strcmp((*elem)->name, class->name) == 0, &id)) {
        
        rcparray_unref(classes, id);
        return -1;
    }

    rcparray_add(classes, class);
    rcparray_unref(buses, id);

    return 0;
}

int dev_set_class(device_t* device, char* class) {
    int id = -1;
    class_t** _class = rcparray_find(classes, class_t*, 
                                strcmp((*elem)->name, class) == 0, &id);

    if (*_class == NULL)
        return -1;

    int ret = (*_class)->bind(device);
    if (ret < 0) {
        rcparray_unref(classes, id);
        return ret;
    }
    snprintf(device->class, sizeof(device->class), "%s", class);

    return 0;
}

void dev_debug() {
    static const char* names[] = {"Memory", "IO", "IRQ", "Other"};

    bus_t* bus;
    device_t* dev;
    dattribute_t* dattr;
    dresource_t* dress;

    size_t bid, did, fid;
    rcparray_foreach(buses, bid) {
        bus = rcparray_get(buses, bid);
        printk("Bus %s:\n", bus->name);

        rcparray_foreach(bus->devices, did) {
            dev = rcparray_get(bus->devices, did);
            printk("  %s:\n", dev->name);

            printk("    attr:\n");
            rcparray_foreach(dev->attributes, fid) {
                dattr = rcparray_get(dev->attributes, fid);
                printk("      %s - %li\n", dattr->name, dattr->value);
                rcparray_unref(dev->attributes, fid);
            }
            
            printk("    ress:\n");
            rcparray_foreach(dev->resources, fid) {
                dress = rcparray_get(dev->resources, fid);

                if (dress->type == DRT_IRQ)
                    printk("      %s - %li\n", names[dress->type], dress->value);
                else
                    printk("      %s - 0x%lx, 0x%lx\n", names[dress->type], dress->start, dress->end);

                rcparray_unref(dev->resources, fid);
            }

            rcparray_unref(bus->devices, did);
        }
        rcparray_unref(buses, bid);
    }
}