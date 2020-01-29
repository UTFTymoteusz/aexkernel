#pragma once

// AEX return codes
#include "aex/vals/rcode_names.h"

#define RETURN_IF_ERROR(x) if (x < 0) { return x; }
#define IF_ERROR(x) if (x < 0)