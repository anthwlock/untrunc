#ifndef MXF_H
#define MXF_H

#include <string>

class MxfRepair {
public:
    static void repair(const std::string& reference, const std::string& damaged,
                       const std::string& destination = std::string());
};

#endif