#include "alma.hpp"
#include <fstream>
#include <cstdio>
#include <sstream>

char memPath[64];
char mapsPath[64];

unsigned int currentPID;

std::vector<unsigned char> alma::memRead(const memAddr address, const unsigned int count) {

    std::ifstream mem(memPath);
    std::vector<unsigned char> retVec(count);

    mem.seekg(address);
    mem.read(reinterpret_cast<char*>(&retVec[0]), count);
    return retVec;
}

void alma::memWrite(const memAddr address, const std::vector<unsigned char> values) {
    std::ofstream mem(memPath, std::ios::out | std::ios::binary);

    mem.seekp(address);
    mem.write(reinterpret_cast<const char*>(&values[0]), values.size());            
}

std::vector<memoryPage> alma::getMemoryPages(const int targetPerms, const int excludedPerms) {
    std::vector<memoryPage> retVec;
    std::ifstream maps_file(mapsPath);
    std::string line;

    // Iterate through each line
    while (std::getline(maps_file, line)) {
        std::istringstream sstream(line);

        unsigned long long pageBegin, pageEnd;
        char pagePerms[4];
        char dash;

        // Parse the line
        sstream >> std::hex >> pageBegin >> dash >> pageEnd >> pagePerms;
        
        memoryPage page {
            memoryPermission_NONE,
            pageBegin,
            pageEnd
        };

        if (pagePerms[0] == 'r') page.permissions |= memoryPermission_READ;
        if (pagePerms[1] == 'w') page.permissions |= memoryPermission_WRITE;
        if (pagePerms[2] == 'x') page.permissions |= memoryPermission_EXECUTE;
        if (pagePerms[3] == 's') page.permissions |= memoryPermission_SHARED;
        if (pagePerms[3] == 'p') page.permissions |= memoryPermission_PRIVATE;

        // Check for excluded permissions
        if (page.permissions & excludedPerms)
            continue;

        // If target permissions are not specified, push back every page.
        if (targetPerms == memoryPermission_NONE)
            retVec.push_back(page);

        // If target permissions specified, only push back pages that meet the filter
        else if (targetPerms & page.permissions)
            retVec.push_back(page);
    }

    return retVec;
}

bool alma::openProcess(const unsigned int pid) {
    snprintf(memPath, 64, "/proc/%i/mem", pid);
    snprintf(mapsPath, 64, "/proc/%i/maps", pid);

    std::ifstream existsCheck(memPath); 
    
    if (existsCheck.is_open())
        currentPID = pid;
        
    return currentPID == pid;
}

std::vector<memAddr> alma::patternScanUChar(const memAddr minLimit, const memAddr maxLimit, std::vector<unsigned char> pattern, const unsigned int alignment, const unsigned int maxOcurrences) {
    std::vector<unsigned short> castedPattern;

    for (auto &elem : pattern)
        castedPattern.push_back(elem);

    return alma::patternScan(minLimit, maxLimit, castedPattern, alignment, maxOcurrences);
}

std::vector<memAddr> alma::patternScan(const memAddr minLimit, const memAddr maxLimit, std::vector<unsigned short> pattern, const unsigned int alignment, const unsigned int maxOcurrences) {

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

unsigned int alma::getCurrentlyOpenPID() {
    return currentPID;
}
