#pragma once

#include "common.h"
#include "file.h"

bool isPointingAtRtmdHeader(FileRead& file);
bool isRtmdHeader(const uchar* buff);