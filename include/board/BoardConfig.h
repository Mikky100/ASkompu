#pragma once

#if defined(ASKOMPU_BOARD_LILYGO_MAIN) && defined(ASKOMPU_BOARD_ILI9488_MAIN)
#error "Select exactly one ASkompu board profile"
#elif defined(ASKOMPU_BOARD_LILYGO_MAIN)
#include "LilygoMainConfig.h"
#elif defined(ASKOMPU_BOARD_ILI9488_MAIN)
#include "Esp32S3Ili9488Config.h"
#else
#error "No ASkompu board profile selected (ASKOMPU_BOARD_LILYGO_MAIN or ASKOMPU_BOARD_ILI9488_MAIN)"
#endif
