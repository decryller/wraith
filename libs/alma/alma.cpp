// alma v1.1
#include "alma.hpp"
#include <fstream>
#include <sstream>

char memPath[64];
char mapsPath[64];

unsigned int currentPID;

std::vector<unsigned char> alma::memRead(const memAddr address, const unsigned int count) {

    std::ifstream mem(memPath, std::ios::in | std::ios::binary);
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

        memAddr pageBegin, pageEnd;
        char dash;
        char pagePerms[4];

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

    std::ifstream existsCheck(memPath, std::ios::in | std::ios::binary); 
    
    if (existsCheck.is_open())
        currentPID = pid;
        
    return currentPID == pid;
}

unsigned int alma::getCurrentlyOpenPID() {
    return currentPID;
}