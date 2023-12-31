#include "libs/alma/alma.hpp"
#include "libs/rvmt/rvmt.hpp"
#include "libs/pcg-cpp/pcg_random.hpp"

#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <random>
#include <X11/Xlib.h>

Display* rootDisplay;
Window rootWindow;

enum clientType {
    clientType_UNKNOWN,
	clientType_FORGE1, // Forge 1.7.10
	clientType_FORGE2, // Forge 1.8.9
    clientType_LUNAR1, // Lunar 1.7.10
    clientType_LUNAR2 // Lunar 1.8.9
};

bool nineSizedAddresses = false;

clientType clientType_CURRENT = clientType_UNKNOWN;
std::string clientType_TITLE;

int __NULLINT;
unsigned int __NULLUINT;
unsigned long __NULLULONG;

bool leftEnabled = false;
bool leftContainerClicks = false;
bool leftAllowedSlots[9];
float lCPS = 10.0;

bool rightEnabled = false;
bool rightSprintOnly = false;
bool rightAllowedSlots[9];
float rightDelay = 250;

bool reachEnabled = false;
bool reachSprintOnly = false;
bool reachAllowedSlots[9];
float reachVal = 3.0;

bool isPlayerSprinting = false;
bool isInContainer = false;
bool isGamePaused = false;

unsigned char activeSlot = 0;

std::atomic<bool> destructing = false;
std::atomic<bool> leftThreadDone = false;
std::atomic<bool> rightThreadDone = false;
std::atomic<bool> reachThreadDone = false;
std::atomic<bool> playerPtrThreadDone = false;

void leftThreadFunc();
void rightThreadFunc();
void reachThreadFunc();
void playerPointerThreadFunc();

float random_float(float range_min, float range_max);
int random_int(int range_min, int range_max);
int randomizer(float cps);

void mouseButtonInstruction(int mouseButton, int instruction);
bool isMouseButtonHeld(unsigned int mouseButton);
bool isHotbarEnabled(bool* var);
bool isActiveWindowMinecraft();

enum GUIPages {
	GUIPages_COMBAT,
	GUIPages_MISC,
	GUIPages_SETTINGS,
};
GUIPages GUIPages_CURRENT;

int main(int argc, char** argv) {
	// Start X11 Resources.
	rootDisplay = XOpenDisplay(NULL);
	rootWindow = XDefaultRootWindow(rootDisplay);

	// === Check for root privileges.
	std::ifstream file("/proc/self/status", std::ios::binary | std::ios::in);
	std::stringstream sstream;
	sstream << file.rdbuf();

	// From what i've read, this condition will work on most distros.
	if (sstream.str().find("Uid:\t0\t0\t0\t0") == std::string::npos) {
		std::cout << "No root privileges. Remember to switch to the root user.";
		return 1; // No root privileges
	}

	// === Automatically detect Minecraft's PID
	unsigned int clientPID = 0;
	for (const auto& folder : std::filesystem::directory_iterator(std::filesystem::path("/proc"))) 
	if (std::filesystem::is_directory(folder)) {

		// Dump comm contents into a string to find the version.
		std::ifstream ifstream(folder.path() / "comm");
		std::string string;
		std::getline(ifstream, string);
		ifstream.close();

		// Check if it's a java process
		if (string == "java") {
			ifstream.open(folder.path() / "cmdline");
			std::getline(ifstream, string);
			ifstream.close();

			// Check for Lunar
			if (string.find(",lunar.") != std::string::npos) {
				// 1.7.10
				if (string.find("OptiFine_v1_7.jar") != std::string::npos) {
					clientType_CURRENT = clientType_LUNAR1;
					clientType_TITLE = "Client 1.7.10";
					clientPID = std::stoi(folder.path().filename().string());
					break;
				}
				// 1.8.9
				if (string.find("OptiFine_v1_8.jar") != std::string::npos) {
					clientType_CURRENT = clientType_LUNAR2;
					clientType_TITLE = "Client 1.8.9";
					clientPID = std::stoi(folder.path().filename().string());
					break;
				}
			}

			// Check for Forge 1.7.10.
			if (string.find("minecraftforge/forge/1.7.10") != std::string::npos) {
				clientType_CURRENT = clientType_FORGE1;
				clientType_TITLE = "Minecraft 1.7.10";
				clientPID = std::stoi(folder.path().filename().string());
				break;
			}

			// Check for Forge 1.8.9.
			if (string.find("minecraftforge/forge/1.8.9") != std::string::npos) {
				clientType_CURRENT = clientType_FORGE2;
				clientType_TITLE = "Minecraft 1.8.9";
				clientPID = std::stoi(folder.path().filename().string());
				break;
			}

		}
	}

	if (clientPID == 0) { // Minecraft couldn't be detected automatically.
		std::cout << "Couldn't find a compatible Minecraft version.";
		return 1;
	}

	if (!alma::openProcess(clientPID)) { // Can't access the pid's memory file
		std::cout << "Error while opening Minecraft's memory file.";
		return 1;
	}

	// === Check addresses length.
	std::vector<memoryPage> _STARTUPPROCESSPAGES = alma::getMemoryPages(memoryPermission_NONE, memoryPermission_NONE);
        
	for (const memoryPage &page : _STARTUPPROCESSPAGES) { 
		if (page.begin >= 0x700000000) {
			nineSizedAddresses = page.end <= 0x800000000;
			break;
		}
	}
	_STARTUPPROCESSPAGES.clear();

	// Launch threads.
	std::thread playerPointerThread(&playerPointerThreadFunc);
	playerPointerThread.detach();

	std::thread leftThread(&leftThreadFunc);
	leftThread.detach();

	std::thread reachThread(&reachThreadFunc);
	reachThread.detach();

	std::thread rightThread(&rightThreadFunc);
	rightThread.detach();

    RVMT::Start();
    RVMT::SetTerminalTitle("Wraith v1.0.0");
	
    while (!destructing.load()) {
        RVMT::BeginFrame();
    	
        const unsigned short rowCount = RVMT::GetRowCount();
        const unsigned short colCount = RVMT::GetColCount();

		switch (GUIPages_CURRENT) {
		static bool noSliders = false;
		case GUIPages_COMBAT:
			RVMT::DrawBox(1, 0, 40, 10); // Autoclicker box

			RVMT::DrawBox(44, 0, 40, 10); // Rightclicker box

			RVMT::DrawBox(1, 12, 40, 10); // Reach box

			// === Autoclicker
			RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 3);
			RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 1);
			RVMT::Text("Autoclicker ");

			RVMT::SameLine();
			RVMT::Checkbox("[ON]", "[OFF]", &leftEnabled);

			RVMT::SetCursorY(NewCursorPos_ADD, 1); // Extra padding
			RVMT::Text("CPS: ");

			RVMT::SameLine();
			
			if (!noSliders) {
				RVMT::Slider("lCPS slider", 20, 10.0, 0.5, &lCPS);
				RVMT::SameLine();
				RVMT::Text(" %.1f", lCPS);
			}

			else {
				static char textInputBuffer[8] = "10,0";

				RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::PushPropertyForNextItem(WidgetProp_InputText_Charset, "0123456789,");
				RVMT::InputText("lCPS field", textInputBuffer, 7, 8);

				RVMT::SameLine();

				if (RVMT::Button("Apply")) {
					const float newVal = std::stof(&textInputBuffer[0]);
					lCPS = newVal > 20 ? 20 : newVal < 10 ? 10 : newVal;

					RVMT::SameLine();
					RVMT::SetCursorY(NewCursorPos_ADD, 1);
					RVMT::Text("Applied");
				}
			}

			RVMT::SetCursorY(NewCursorPos_ADD, 1);
			RVMT::Text("Click on containers ");

			RVMT::SameLine();
			RVMT::Checkbox("[Enabled]", "[Disabled]", &leftContainerClicks);

			RVMT::SetCursorY(NewCursorPos_ADD, 1);
			RVMT::Text("Allowed hotbar slots");

			for (int i = 0; i < 9; i++) {
				RVMT::Checkbox("[X] ", "[-] ", &leftAllowedSlots[i]);
				if (i < 8)
					RVMT::SameLine();
			}

			// === Rightclicker
			RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 46);
			RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 1);
			RVMT::Text("Rightclicker ");

			RVMT::SameLine();
			RVMT::Checkbox("[ON]", "[OFF]", &rightEnabled);
			
			RVMT::SetCursorY(NewCursorPos_ADD, 1); // Extra padding
			RVMT::Text("Delay: ");

			RVMT::SameLine();

			if (!noSliders) {
				RVMT::Slider("rightDelay slider", 20, 100, 50, &rightDelay);

				RVMT::SameLine();
				RVMT::Text(" %.0fms", rightDelay);
			}
			else {
				static char textInputBuffer[8] = "250";

				RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::InputText("rightDelay field", textInputBuffer, 7, 8);
				
				RVMT::SameLine();
				if (RVMT::Button("Apply")) {
					const float newVal = std::stof(&textInputBuffer[0]);
					rightDelay = newVal > 1100 ? 11000 : newVal < 100 ? 100 : newVal;

					RVMT::SameLine();
					RVMT::SetCursorY(NewCursorPos_ADD, 1);
					RVMT::Text("Applied");
				}
			}

			RVMT::SetCursorY(NewCursorPos_ADD, 1);
			RVMT::Text("Allowed hotbar slots");

			for (int i = 0; i < 9; i++) {
				RVMT::Checkbox("[X] ", "[-] ", &rightAllowedSlots[i]);
				if (i < 8)
					RVMT::SameLine();
			}

			// === Reach
			RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 3);
			RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 13);
			RVMT::Text("Reach ");

			RVMT::SameLine();
			RVMT::Checkbox("[ON]", "[OFF]", &reachEnabled);
			
			RVMT::SetCursorY(NewCursorPos_ADD, 1); // Extra padding

			RVMT::Text("Reach: ");

			RVMT::SameLine();

			if (!noSliders) {
				RVMT::Slider("maxReach slider", 20, 3.0, 0.15, &reachVal);
				RVMT::SameLine();
				RVMT::Text(" %.2f", reachVal);
			}
			
			else {
				static char textInputBuffer[8] = "3,0";

				RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::PushPropertyForNextItem(WidgetProp_InputText_Charset, "0123456789,");

				RVMT::InputText("maxReach field", &textInputBuffer[0], 7, 8);
				
				RVMT::SameLine();
				if (RVMT::Button("Apply")) {
					const float newVal = std::stof(&textInputBuffer[0]);
					reachVal = newVal > 6 ? 6 : newVal < 3 ? 3 : newVal;

					RVMT::SameLine();
					RVMT::SetCursorY(NewCursorPos_ADD, 1);
					RVMT::Text("Applied");
				}
			}

			RVMT::SetCursorY(NewCursorPos_ADD, 1);
			RVMT::Text("Only while sprinting ");

			RVMT::SameLine();
			RVMT::Checkbox("[Enabled]", "[Disabled]", &reachSprintOnly);

			RVMT::SetCursorY(NewCursorPos_ADD, 1);
			RVMT::Text("Allowed hotbar slots");

			for (int i = 0; i < 9; i++) {
				RVMT::Checkbox("[X] ", "[-] ", &reachAllowedSlots[i]);
				if (i < 8)
					RVMT::SameLine();
			}
			break;
		
		case GUIPages_MISC:
			RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 1);
			RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 0);
			if (RVMT::Button("Self-Destruct")) {
				// For people who compiled it from source. | Deletes the whole directory if it's called "wraith".
				if (std::filesystem::current_path().filename() == "wraith") 
					std::filesystem::remove_all(std::filesystem::current_path());

				else // For people who just downloaded the AppImage. | Delete the binary regardless of the name.
					std::filesystem::remove(std::filesystem::current_path() / &argv[0][2]);
				
				destructing.store(true);
			};
			break;
		case GUIPages_SETTINGS:
			RVMT::Text("Use input fields instead of sliders ");

			RVMT::SameLine();
			RVMT::Checkbox("[Enabled]", "[Disabled]", &noSliders);
			break;
		}

        RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 0);
        RVMT::SetCursorY(NewCursorPos_ABSOLUTE, rowCount - 3);
		if (RVMT::Button("Combat"))
			GUIPages_CURRENT = GUIPages_COMBAT;

		RVMT::SameLine();
		if (RVMT::Button("Misc"))
			GUIPages_CURRENT = GUIPages_MISC;

		RVMT::SameLine();
		if (RVMT::Button("Settings"))
			GUIPages_CURRENT = GUIPages_SETTINGS;

		RVMT::SameLine();
        RVMT::SetCursorX(NewCursorPos_ABSOLUTE, colCount - 8);
        if (RVMT::Button(" Quit "))
            destructing.store(true);

        RVMT::Render();
        RVMT::WaitForNewInput();
    }

    // !=== Wait for all threads to finish. ===!
    // === playerPtrThreadDone
    // === autoclickerThreadDone
    // === rightclickerThreadDone
    // === reachThreadDone
    while (	!playerPtrThreadDone.load() || !leftThreadDone.load() ||
			!reachThreadDone.load() || !rightThreadDone.load()) {
    	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	XCloseDisplay(rootDisplay);

    RVMT::Stop();
    return 0;
}
// === Module functions definition
void leftThreadFunc() {
	while (!destructing.load()) {

		while (!leftEnabled && !destructing.load()) 
			std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (isMouseButtonHeld(1) && isActiveWindowMinecraft()) {
			while (isActiveWindowMinecraft() && isMouseButtonHeld(1) && random_float(0.0, 1.0) <= random_float(0.997, 1.0)) {
				if (isGamePaused ||
					(!isInContainer && isHotbarEnabled(leftAllowedSlots) && !leftAllowedSlots[activeSlot]) ||
					(!leftContainerClicks && isInContainer))
					continue;

				mouseButtonInstruction(1, 1);
				std::this_thread::sleep_for(std::chrono::milliseconds(randomizer(lCPS)));
				
				mouseButtonInstruction(1, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds(randomizer(lCPS)));

			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(random_int(50, 100)));

    }
	leftThreadDone.store(true);
}

void rightThreadFunc() {
	while (!destructing.load()) {

		while (!rightEnabled && !destructing.load()) 
			std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (isMouseButtonHeld(2) && isActiveWindowMinecraft()) {
			while (isActiveWindowMinecraft() && isMouseButtonHeld(2) && random_float(0.0, 1.0) <= random_float(0.997, 1.0)) {
				if (isGamePaused || isInContainer ||
					(isHotbarEnabled(rightAllowedSlots) && !rightAllowedSlots[activeSlot]))
					continue;

				mouseButtonInstruction(2, 1);
				std::this_thread::sleep_for(std::chrono::milliseconds(random_int(40, 60)));

				mouseButtonInstruction(2, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds((int)rightDelay + random_int(-60, -40)));
				
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds((int)random_float(50.0, 100.0)));

    }
	rightThreadDone.store(true);
}

void reachThreadFunc() {
	memAddr reachAddress = 0xBAD;
    memAddr blockReachAddress = 0xBAD;
    memAddr livelinessAddress = 0xBAD; // Liveliness bytes: [89 84 24 00 c0]

	const double defaultBlockReach = 4.5;
	const float defaultFloatReach = 3.0f;
	const double defaultDoubleReach = 3.0;

	const bool useDoubleReach =
	clientType_CURRENT == clientType_LUNAR1 ||
	clientType_CURRENT == clientType_FORGE1;

	std::vector<unsigned char> defaultReachMemSig = 
		useDoubleReach
		? alma::varToHex(defaultDoubleReach, 8)
		: alma::varToHex(defaultFloatReach, 4);
	
	const std::vector<unsigned char> livelinessSig{0x89, 0x84, 0x24, 0x0, 0xc0};

    while (!destructing.load()) {
        if (!reachEnabled) {
			if (reachAddress != 0xBAD &&
				blockReachAddress != 0xBAD &&
				livelinessAddress != 0xBAD) {
				alma::memWrite(blockReachAddress, alma::varToHex(defaultBlockReach, 8));
				alma::memWrite(reachAddress, defaultReachMemSig);
			}
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // Check if the reach address is still alive.
        if (livelinessAddress != 0xBAD) {
            const std::vector<unsigned char> livelinessBytes = alma::memRead(livelinessAddress, 5);
            if (livelinessBytes != std::vector<unsigned char>{0x89, 0x84, 0x24, 0x0, 0xc0}) 
                livelinessAddress = 0xBAD;
        }

        // Force reach's address reset until a successful one is found.
        while (livelinessAddress == 0xBAD && !destructing.load()) {
            
            // Reach address always resides in pages with rwxp permissions, so you should
            // only search through pages with such permissions.
            const std::vector<memoryPage> pages = alma::getMemoryPages(memoryPermission_READ | memoryPermission_EXECUTE, memoryPermission_NONE);

            // Loop through RWXP memory pages.
            for (const memoryPage &page : pages) 

            // Loop through found block reachs
            for (const memAddr blockReachResult : alma::patternScan(page.begin, page.end, alma::varToHex(defaultBlockReach, 8)))       
            if (blockReachResult != 0xBAD) 

            // Loop through found reachs in a 256 range from block reachs' results.
            for (const memAddr reachResult : alma::patternScan(blockReachResult - 256, blockReachResult + 256, defaultReachMemSig)) 
            if (reachResult != 0xBAD && livelinessAddress == 0xBAD) {
                // Search for a liveliness value near the current reach address.
                const memAddr livelinessResult = alma::patternScan(reachResult - 256, reachResult + 256, livelinessSig, 1, 1)[0];
                
                if (livelinessResult != 0xBAD) 
					blockReachAddress = blockReachResult,
					reachAddress = reachResult,
					livelinessAddress = livelinessResult;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

		float finalReach = reachVal;

		if ((reachSprintOnly && !isPlayerSprinting) ||
			(isHotbarEnabled(reachAllowedSlots) && !reachAllowedSlots[activeSlot]))
			finalReach = defaultFloatReach;

		const double blockReachToWrite = finalReach + 1.5;
		alma::memWrite(blockReachAddress, alma::varToHex(blockReachToWrite, 8));
		
		if (useDoubleReach) { // 1.7.10
			const double reachToWrite = finalReach;
        	alma::memWrite(reachAddress, alma::varToHex(reachToWrite, 8));
		}
		else { // 1.8.9
			const float reachToWrite = finalReach;
        	alma::memWrite(reachAddress, alma::varToHex(reachToWrite, 4));
		}

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

	alma::memWrite(blockReachAddress, alma::varToHex(defaultBlockReach, 8));
	alma::memWrite(reachAddress, defaultReachMemSig);

	reachThreadDone.store(true);
}

void playerPointerThreadFunc() {
	// To support a new version:
	// 1) Find a master signature. Find a reliant way to get to the "pause byte". The rest of the offsets are after this. 
	// 2) Add the following offsets:
	//	2.1) gamePausedOffset			|	Byte set to 10 when the game is paused, 0 when not.
	//	2.2) containerPointerOffset		|	Pointer to the current container.
	//	2.3) playerStructPointerOffset	|	Pointer to the player's "location" struct
	//	2.4) hotbarStructPointerOffset	|	Pointer to the player's hotbar struct (Usually near the player's location struct)
	//	2.5) activeSlotOffset			|	int8 set to the player's active slot.
	//	2.6) isPlayerSprintingOffset	|	Bool set to the player's sprinting status.
	// 3) Find memory ranges, for now, there's two:
	//	3.1) <~2GB: Eight sized addresses.
	//	3.2) >~2GB: Nine sized addresses. If nine sized, just multiply pointers by 8.

	std::vector<unsigned short> masterSigVec;

	memAddr masterSignatureAddress = 0xBAD;

	unsigned char playerStructPointerOffset = 0;
	unsigned char containerPointerOffset = 0;
	unsigned char gamePausedOffset = 0;
	unsigned char activeSlotOffset = 0;

	unsigned short hotbarStructPointerOffset = 0;
	unsigned short isPlayerSprintingOffset = 0;

	switch (clientType_CURRENT) {
		case clientType_FORGE1:
            masterSigVec = {0x89, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x420, 0x420, 0x420, 0x420, 0x89, 0x01, 0x00, 0x00};
            gamePausedOffset = 32;
            playerStructPointerOffset = 112;
                hotbarStructPointerOffset = 0x2A8;
                    activeSlotOffset = 12;
                isPlayerSprintingOffset = 0x34E;
            containerPointerOffset = 0x8C;
			break;

		case clientType_FORGE2:
			masterSigVec = {0x89, 0x01, 0x00, 0x00, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			gamePausedOffset = 0x30;
			playerStructPointerOffset = 0x94;
                hotbarStructPointerOffset = 0x2A4;
                    activeSlotOffset = 12;
                isPlayerSprintingOffset = 0x331;
			containerPointerOffset = 0xB0;
			break;

		case clientType_LUNAR1:
			masterSigVec = {0x89, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x420, 0x420, 0x420, 0x420, 0x89, 0x01, 0x00, 0x00};
			gamePausedOffset = 40;
			playerStructPointerOffset = 120;
				isPlayerSprintingOffset = 655;
				hotbarStructPointerOffset = 656;
					activeSlotOffset = 12;
			containerPointerOffset = 148;
			break;
			
		case clientType_LUNAR2:
			masterSigVec = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x01, 0x00, 0x00, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0x420, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			gamePausedOffset = 76;
			playerStructPointerOffset = 180;
				hotbarStructPointerOffset = 652;
					activeSlotOffset = 12;
				isPlayerSprintingOffset = 804;
			containerPointerOffset = 208;
			break;

		default:
			break;
	}
	
	while (!destructing.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		while (masterSignatureAddress == 0xBAD && !destructing.load()) {
			memAddr minLimit, maxLimit;

			switch (clientType_CURRENT) {
				case clientType_FORGE1:
					minLimit = nineSizedAddresses ? 0x6C0000000 : 0xC0000000;
					maxLimit = nineSizedAddresses ? 0x800000000 : 0xD6000000;
					break;

				case clientType_FORGE2:
					minLimit = nineSizedAddresses ? 0x6C0000000 : 0xC0000000;
					maxLimit = nineSizedAddresses ? 0x800000000 : 0xD6000000;
					break;

				case clientType_LUNAR1:
					minLimit = nineSizedAddresses ? 0x700000000 : 0x80000000;
					maxLimit = nineSizedAddresses ? 0x800000000 : 0x90000000;
					break;

				case clientType_LUNAR2:
					minLimit = nineSizedAddresses ? 0x700000000 : 0x80000000;
					maxLimit = nineSizedAddresses ? 0x800000000 : 0x90000000;
					break;

				default:
					minLimit = 0x0BAD;
					maxLimit = 0x0BAD;
					break;
			}

			unsigned char masterSigAlig = 8;

			if (clientType_CURRENT == clientType_LUNAR1 ||
				clientType_CURRENT == clientType_FORGE1)
				masterSigAlig = 4;

			masterSignatureAddress = alma::patternScan (
				minLimit, maxLimit,
				masterSigVec, masterSigAlig, 1
			)[0];
		}

		memAddr playerStructPointer = alma::hexToVar<unsigned int>(alma::memRead(masterSignatureAddress + playerStructPointerOffset, 4));
		if (nineSizedAddresses) playerStructPointer *= 8;

		memAddr hotbarStructAddress = alma::hexToVar<unsigned int>(alma::memRead(playerStructPointer + hotbarStructPointerOffset, 4));
		if (nineSizedAddresses) hotbarStructAddress *= 8;

		isPlayerSprinting 	= alma::memRead(playerStructPointer + isPlayerSprintingOffset, 1)[0];
		isInContainer 		= !isGamePaused && alma::hexToVar<int>(alma::memRead(masterSignatureAddress + containerPointerOffset, 4)) != 0;
		isGamePaused 		= alma::memRead(masterSignatureAddress + gamePausedOffset, 1)[0] == 0x10;
		
		activeSlot 			= alma::memRead(hotbarStructAddress + activeSlotOffset, 1)[0];
	}
	playerPtrThreadDone.store(true);
}

// === Random functions definition
float random_float(float range_min, float range_max) {
	pcg32 rng(pcg_extras::seed_seq_from<std::random_device>{});
	std::uniform_real_distribution<float> dist(range_min, range_max);
	return dist(rng);
}

int random_int(int range_min, int range_max) {
	pcg32 rng(pcg_extras::seed_seq_from<std::random_device>{});
	std::uniform_int_distribution<int> dist(range_min, range_max);
	return dist(rng);
}

int randomizer(float cps) {
	// Randomized enough to not flag. Needs improvement anyway.
	if (random_float(0, 1) < random_float(0.87, 1))
		return 500 / random_float(cps - random_float(2.41, 3.34), cps + random_float(3.53, 4.68));
	else
		return 500 / random_float(cps - random_float(6.44, 8.35), cps - random_float(2.52, 3.81));
}

// === Misc functions definition
void mouseButtonInstruction(int mouseButton, int instruction) {

    XEvent event;
    event.type = ButtonPress;

    switch (mouseButton) {
        case 1: event.xbutton.button = Button1; break;
        case 2: event.xbutton.button = Button3; break;
        default: return; break;
    }

    if (instruction == 0) 
        event.type = ButtonRelease;

    event.xbutton.same_screen = True;
    event.xbutton.subwindow = rootWindow;

    while (event.xbutton.subwindow) {
        event.xbutton.window = event.xbutton.subwindow;
        XQueryPointer(rootDisplay, event.xbutton.window, &event.xbutton.root, &event.xbutton.subwindow,
                      &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y,
                      &event.xbutton.state);
    }

    XSendEvent(rootDisplay, PointerWindow, True, ButtonPressMask | ButtonReleaseMask, &event);
    XFlush(rootDisplay);
}

bool isMouseButtonHeld(unsigned int mouseButton) {
    unsigned int buttons = 0;
	unsigned int mouseMask = 0;
	switch (mouseButton) {
		case 1:
			mouseMask = Button1Mask;
			break;
		case 2:
			mouseMask = Button3Mask;
			break;
	}
    XQueryPointer(rootDisplay, rootWindow, &__NULLULONG, &__NULLULONG,
                  &__NULLINT, &__NULLINT, &__NULLINT, &__NULLINT, &buttons);
    return (buttons & mouseMask);
}

bool isHotbarEnabled(bool* var) {
	for (int i = 0; i < 9; i++)
		if (var[i])
			return true;
	return false;
}

bool isActiveWindowMinecraft() {
    Window currentWindow;
    char* buffer = nullptr;
	bool rvalue = false;

    XGetInputFocus(rootDisplay, &currentWindow, &__NULLINT);
    if (XFetchName(rootDisplay, currentWindow, &buffer) > 0) {
        std::string title(buffer);
		rvalue = title.find(clientType_TITLE) != std::string::npos;
        XFree(buffer);
    }
    return rvalue;
}
