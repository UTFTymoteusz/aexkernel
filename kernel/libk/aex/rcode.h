#pragma once

// AEX return codes

enum aex_return_code {
    ERR_GENERAL       = -0x000001,
    ERR_INV_ARGUMENTS = -0x000002,
    ERR_NO_SPACE      = -0x000003,
    ERR_ALREADY_DONE  = -0x000004,
    ERR_NOT_POSSIBLE  = -0x000005,

    ERR_NOT_FOUND     = -0x000006,
    DEV_ERR_NOT_FOUND = -0x0D0006,
    FS_ERR_NOT_FOUND  = -0x0F0006,

    FS_ERR_NO_MATCHING_FILESYSTEM = -0x0F0007,

    ERR_UNKNOWN       = -0x0000FF,
};