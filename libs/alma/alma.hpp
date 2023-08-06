#ifndef ALMA_HPP
#define ALMA_HPP

#include <vector>
typedef unsigned long long memAddr;

enum memoryPermission_ {
    memoryPermission_NONE = 0,
    memoryPermission_READ = 1 << 0,
    memoryPermission_WRITE = 1 << 1,
    memoryPermission_EXECUTE = 1 << 2,
    memoryPermission_PRIVATE = 1 << 3,
    memoryPermission_SHARED = 1 << 4,
};

struct memoryPage {
    int permissions = memoryPermission_NONE;
    unsigned long long begin = 0;
    unsigned long long end = 0;
};

namespace alma {
    extern std::vector<unsigned char> memRead(const memAddr address, const unsigned int count);
    extern void memWrite(const memAddr address, const std::vector<unsigned char> values);
    
    extern std::vector<memoryPage> getMemoryPages (const int targetPerms, const int excludedPerms);
    
    // Sets up "memPath" and "mapsPath"
    // Returns true if process' mem file is accessible.
    extern bool openProcess(const unsigned int pid);

    // Scan between a given range for a specific byte pattern.
    // Use "x420" for wildcards.
    extern std::vector<memAddr>patternScan(const memAddr minLimit, const memAddr maxLimit, std::vector<unsigned short> pattern, const unsigned int alignment = 1, const unsigned int maxOcurrences = 0);
    
    // Scan between a given range for a specific byte pattern.
    extern std::vector<memAddr>patternScanUChar(const memAddr minLimit, const memAddr maxLimit, std::vector<unsigned char> pattern, const unsigned int alignment = 1, const unsigned int maxOcurrences = 0);

    extern unsigned int getCurrentlyOpenPID();
}
#endif