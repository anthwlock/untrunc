#ifndef PCH_H
#define PCH_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <random>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <codecvt>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavutil/ffversion.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
}

// #include <ui.h>

#endif // PCH_H
