// alma v1.1
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
    memAddr begin = 0;
    memAddr end = 0;
};

namespace alma {
    // Read bytes at a specific address.
    // Returns values residing at the specified address.
    extern std::vector<unsigned char> memRead(const memAddr address, const unsigned int count);

    // Write bytes to a specific address.
    extern void memWrite(const memAddr address, const std::vector<unsigned char> values);

    // Get memory pages
    // Can return an empty vector.
    extern std::vector<memoryPage> getMemoryPages(const int targetPerms, const int excludedPerms);
    
    // Sets up "memPath" and "mapsPath"
    // Returns true if pid's mem file is accessible.
    extern bool openProcess(const unsigned int pid);

    // Scan between a given range for a specific byte pattern.
    // Use 0x420 for wildcards.
    // If no matches were found, {0xBAD} will be returned.
    template <typename val>
    std::vector<memAddr> patternScan(const memAddr minLimit, const memAddr maxLimit, std::vector<val> pattern, const unsigned int alignment = 4, const unsigned int maxOcurrences = 0) {
        std::vector<memAddr> retVec{0xBAD};
        int chunkSize = 0x100000; // Optimal chunk size. Not much difference when increased / decreased away from this.

        const int patternSize = pattern.size(); // Use a variable instead of calling the function.
        for (memAddr currentAddress = minLimit; currentAddress < maxLimit; currentAddress += chunkSize) {

            // Prevent going out of maxLimit bounds.
            if (currentAddress + chunkSize >= maxLimit)
                chunkSize = maxLimit - currentAddress;

            const std::vector<unsigned char> chars = memRead(currentAddress, chunkSize);

            for (int x = 0; x < chunkSize - patternSize; x += alignment) // Chunk reading loop
                for (int y = 0; y < patternSize; y++) { // Pattern matching loop
                    // Skip wildcards
                    if (pattern[y] == 0x420)
                        continue;

                    if (chars[x + y] != pattern[y])
                        break;

                    // Successful match
                    if (y == patternSize - 1) {
                        if (retVec[0] == 0xBAD) // Replace error element
                            retVec[0] = currentAddress + x;

                        else
                            retVec.push_back(currentAddress + x);

                        if (retVec.size() == maxOcurrences)
                            return retVec;
                    }
                }
        }
        return retVec;
    }
    // Scan all pages that have a permission included in targetPerms.
    // Ignores all pages that have a permission included in excludedPerms.
    // Use 0x420 for wildcards.
    // If no matches were found, {0xBAD} will be returned.
    template <typename val>
    std::vector<memAddr> patternScanPerms(const int targetPerms, const int excludedPerms, std::vector<val> pattern, const unsigned int alignment = 4, const unsigned int maxOcurrences = 0) {
        std::vector<memAddr> retVec{0xBAD};
        for (const memoryPage page : getMemoryPages(targetPerms, excludedPerms)) {
            const auto result = patternScan(page.begin, page.end, pattern, alignment, maxOcurrences);

            if (result[0] == 0xBAD)
                continue;

            if (retVec[0] == 0xBAD)
                retVec = result; // Replace error element.
            else
                retVec.insert(retVec.end(), result.begin(), result.end());
        }
        return retVec;
    }

    // Get PID of currently open process.
    // Returns 0 if none is open.
    extern unsigned int getCurrentlyOpenPID();

    // Use this only if you know what you're doing.
    template <typename varType>
    varType hexToVar(std::vector<unsigned char> bytes) {
        varType rvalue;
        
        for (int i = 0; i < bytes.size(); i++)
            ((char*)&rvalue)[i] = bytes[i];

        return rvalue;
    };

    // Use this only if you know what you're doing.
    template <typename varType>
    std::vector<unsigned char> varToHex(varType &var, unsigned int varMemSize) {
        std::vector<unsigned char> rvalue(varMemSize);

        for (int i = 0; i < varMemSize; i++)
            rvalue[i] = ((char*)&var)[i];

        return rvalue;
    };
}
#endif