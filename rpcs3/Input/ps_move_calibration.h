#pragma once
#ifdef HAVE_HIDAPI

#include "ps_move_handler.h"

void psmove_parse_calibration(const reports::ps_move_calibration_blob& calibration, ps_move_device& device);

#endif // HAVE_HIDAPI
