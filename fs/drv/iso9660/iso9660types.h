#pragma once

#include "aex/aex.h"
#include <stdint.h>

enum iso9660_vtype {
    ISO9660_BOOT_RECORD = 0,
    ISO9660_PRIMARY_VOLUME = 1,
    ISO9660_SUPPLEMENTARY_VOLUME = 2,
    ISO9660_VOLUME_PARTITION = 3,
    ISO9660_TERMINATOR = 255,
};

typedef int16_t int16le_t;
typedef int16_t int16be_t;

typedef uint16_t uint16le_t;
typedef uint16_t uint16be_t;

typedef struct int16lebe {
    int16_t le;
    int16_t be;
} PACKED int16lebe_t;
typedef struct uint16lebe {
    uint16_t le;
    uint16_t be;
} PACKED uint16lebe_t;

typedef int32_t int32le_t;
typedef int32_t int32be_t;

typedef uint32_t uint32le_t;
typedef uint32_t uint32be_t;

typedef struct int32lebe {
    int32_t le;
    int32_t be;
} PACKED int32lebe_t;
typedef struct uint32lebe {
    uint32_t le;
    uint32_t be;
} PACKED uint32lebe_t;

typedef struct iso9660datetime {
    char year[4];
    char month[2];
    char day[2];

    char hour[2];
    char minute[2];
    char second[2];
    char millisecond[2];

    int8_t gmt_offset;
} PACKED iso9660datetime_t;

struct iso9660_vdesc {
    uint8_t type;
    char identifier[5];
    uint8_t version;

    uint8_t data[2041];
} PACKED;

struct iso9660_boot_record {
    uint8_t type;
    char identifier[5];
    uint8_t version;

    char boot_system_id[32];
    char boot_id[32];

    uint8_t boot_data[1977];
} PACKED;

typedef struct iso9660datetimec {
    uint8_t year;
    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    int8_t gmt_offset;
} PACKED iso9660datetimec_t;

typedef struct iso9660_dentry {
    uint8_t len;
    uint8_t ext_attr_len;

    uint32lebe_t data_lba;
    uint32lebe_t data_len;

    iso9660datetimec_t datetime;
    uint8_t flags;

    uint8_t int_unit_size;
    uint8_t int_gap_size;

    uint16lebe_t volume_sequence_number;
    
    uint8_t filename_len;
    char filename[];
} PACKED iso9660_dentry_t;

struct iso9660_primary_volume_desc {
    uint8_t type;
    char identifier[5];
    uint8_t version;

    uint8_t unused0;

    char system_id[32];
    char volume_id[32];

    uint8_t unused1[8];
    uint32lebe_t volume_space_size;

    uint8_t unused2[32];
    uint16lebe_t volume_set_size;
    uint16lebe_t volume_sequence_number;
    uint16lebe_t logical_block_size;
    uint32lebe_t path_table_size;

    uint32le_t lpath_table;
    uint32le_t loptpath_table;
    uint32be_t bpath_table;
    uint32be_t boptpath_table;

    iso9660_dentry_t root;
    char root_filename[1];

    char volume_set_identifier[128];
    char publisher_identifier[128];
    char data_preparer_identifier[128];
    char application_identifier[128];

    char copyright_file_identifier[38];
    char abstract_file_identifier[36];
    char bibliographic_file_identifier[37];

    iso9660datetime_t creation_dt;
    iso9660datetime_t modification_dt;
    iso9660datetime_t expiration_dt;
    iso9660datetime_t effective_dt;

    uint8_t file_structure_version;
    uint8_t unused3;

    uint8_t application_used[512];
    uint8_t reserved[653];
} PACKED;