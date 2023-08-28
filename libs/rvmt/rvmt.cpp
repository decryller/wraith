#include "rvmt.hpp"

#include <sys/ioctl.h>
#include <algorithm>
#include <termios.h>
#include <iostream>
#include <cstdarg>
#include <sstream>
#include <thread>
#include <vector>

#define CHRONOELAPSEDMS(HIGH_RES_TIMEPOINT) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - HIGH_RES_TIMEPOINT).count()

// Enums and structs
enum MouseAction {
	MouseAction_NONE		= 0,

	MouseAction_LEFTPRESS	= 1,
	MouseAction_MIDDLEPRESS	= 2,
	MouseAction_RIGHTPRESS	= 3,

	MouseAction_LEFTDRAG	= 4,
	MouseAction_MIDDLEDRAG	= 5,
	MouseAction_RIGHTDRAG	= 6,

	MouseAction_SCROLLUP	= 7,
	MouseAction_SCROLLDOWN	= 8,

	MouseAction_RELEASE		= 9
};

enum InputType {
	InputType_NONE,
	InputType_KEYBOARD,
	InputType_MOUSE
};

struct mouseInput {
	unsigned char col;
	unsigned char row;
	MouseAction action;
};

struct keyPress {
	char key;
	const char* field;

	keyPress operator=(keyPress from) const {
		return {from.key, from.field};
	}
};

struct customProperty {
	WidgetProp property = WidgetProp_NULL_RVMT_WIDGET_PROPERTY;
	int intVal = 0;
	const char* strVal = nullptr;
};

// Globals
bool sameLinedPreviousItem	= false;
bool sameLineCalled			= false;

unsigned short colCount	= 0; // X axis.
unsigned short rowCount	= 0; // Y axis.

int cursorX = 0;
int cursorY = 0;

int sameLineXRevert	= 0;
int sameLineYRevert	= 0;
int sameLineX		= 0;
int sameLineY		= 0;

BoxStyle BoxStyle_Current	= BoxStyle_Round;
ItemType activeItemType		= ItemType_None;

InputType latestInputType   = InputType_NONE;

const char* activeItemID = "none";
mouseInput* latestMouseInput;
// Input fields manage keypresses.

std::vector<customProperty> PushedProperties;

std::vector<std::wstring> canvas{L""}; // Initialize an empty canvas.
std::vector<keyPress> KEYPRESSES;
std::vector<mouseInput> mouseInputs;

// Utility funcs
bool strEquals(const char* first, const char* second) {
	for (int i = 0; i < 4096; i++) {
		if (first[i] == 0 && second[i] == 0)
			return true;

		if (first[i] != second[i])
			return false;
	}
	return false;
}

unsigned short strLength(const char* str) {
	if (str[0] == 0)
		return 0;

	for (int i = 0; i < 4096; i++) 
		if (str[i] == 0)
			return i;
	
	return 4096;
}

bool strContains(const char* str, char ch) {

	for (int i = 0; i < 4096; i++) {
		if (str[i] == 0)
			return false;

		if (str[i] == ch)
			return true;
	}
	return false;
}

void ResetActiveItem() {
	activeItemType = ItemType_None;
	activeItemID = "none";
}

void SetActiveItem(const ItemType type) {
	activeItemType = type;
	activeItemID = "none";
}

void SetActiveItem(const ItemType type, const char* ID) {
	activeItemType = type;
	activeItemID = ID;
}

void WidgetCursorHandling() {
	if (sameLineCalled)
		cursorX = sameLineX,
		cursorY = sameLineY,
		sameLineCalled = false,
		sameLinedPreviousItem = true;

	else if (sameLinedPreviousItem)
		cursorX = sameLineXRevert,
		cursorY = sameLineYRevert,
		sameLinedPreviousItem = false;
}

void InternalSetCursor(char axis, NewCursorPos mode, int value) {
	int *cursor;
	
	if (axis == 'x') {
		if (sameLineCalled)
			cursor = &sameLineX;
		
		else if (!sameLineCalled && sameLinedPreviousItem)
			cursor = &sameLineXRevert;

		else
			cursor = &cursorX;
	}
	
	if (axis == 'y') {
		if (sameLineCalled)
			cursor = &sameLineY;
		
		else if (!sameLineCalled && sameLinedPreviousItem)
			cursor = &sameLineYRevert;

		else
			cursor = &cursorY;
	}

	switch (mode) {
		case NewCursorPos_ADD:
			*cursor += value;
			break;

		case NewCursorPos_SUBTRACT:
			*cursor -= value;
			break;

		case NewCursorPos_ABSOLUTE:
			*cursor = value;
			break;
	}
}

bool CanvasPosExists(const unsigned short x, const unsigned short y) {
	return (
		y < canvas.size() &&
		x < canvas[y].length());
}

void PushToCanvas(const unsigned short x, const unsigned short y, const wchar_t ch) {
	if (CanvasPosExists(x, y))
		canvas[y][x] = ch;
}

using namespace RVMT;

// !=== Widgets ===!
// === RVMT::Text
// === RVMT::Checkbox
// === RVMT::Button
// === RVMT::Slider
// === RVMT::InputText

void RVMT::Text(const char* val, ...) {
	WidgetCursorHandling();
		
	char buffer[1024];

	va_list args;
	va_start(args, val);
	const int textLength = vsnprintf(buffer, 1024, val, args);
	va_end(args);

	DrawString(cursorX, cursorY, buffer);

	sameLineX = cursorX + textLength;
	sameLineY = cursorY;

	cursorY += 1 + std::count(&val[0], &val[textLength], 10); // Count newlines
}

bool RVMT::Checkbox(const char* trueText, const char* falseText, bool* val) {
	WidgetCursorHandling();
	bool rvalue = false;

	const char* ptr = *val ? &trueText[0] : &falseText[0];
	unsigned short textWidth = strLength(ptr);

	// Handle input.
	if (latestInputType == InputType_MOUSE &&
		latestMouseInput->action == MouseAction_LEFTPRESS &&
		latestMouseInput->col >= cursorX && latestMouseInput->col < cursorX + textWidth &&
		latestMouseInput->row == cursorY) {
		
		*val = !*val;
		ptr = *val ? &trueText[0] : &falseText[0];

		SetActiveItem(ItemType_Checkbox);
		rvalue = true;
	}

	DrawString(cursorX, cursorY, ptr);

	// Handle cursor and SameLine.
	sameLineX = cursorX + textWidth;
	sameLineY = cursorY;

	cursorY++;

	return rvalue;
}

bool RVMT::Button(const char* str, ...) {
	WidgetCursorHandling();

	bool drawTextOnly = false;
	for (auto &prop : PushedProperties)
		if (prop.property == WidgetProp_Button_TextOnly)
			drawTextOnly = true;

	// Parse argument list
	char buffer[1024];
	va_list args;
	va_start(args, str);
	const auto textLength = vsnprintf(buffer, sizeof(buffer), str, args);
	va_end(args);

	const int startX = cursorX;
	const int startY = cursorY;
	const unsigned short buttonHeight = drawTextOnly ? 1 : 3;
	const unsigned short buttonWidth = drawTextOnly ? textLength : textLength + 2;

	if (!drawTextOnly) {
		DrawBox(cursorX, cursorY, textLength, 1);

		// Prepare cursor for text to be drawn at the box's middle
		cursorX++;
		cursorY++;
	}

	DrawString(cursorX, cursorY, buffer);

	cursorX = startX;
	cursorY = startY + buttonHeight;

	sameLineX = startX + buttonWidth;
	sameLineY = startY;

	PushedProperties.clear();

	// Handle return value
	if (latestInputType == InputType_MOUSE &&
		latestMouseInput->action == MouseAction_LEFTPRESS &&
		latestMouseInput->col >= startX && latestMouseInput->col < startX + buttonWidth &&
		latestMouseInput->row >= startY && latestMouseInput->row < startY + buttonHeight) {

		SetActiveItem(ItemType_Button);
		return true;
	}
	return false;
}

void RVMT::Slider(const char* sliderID, unsigned short ticks, float minVal, float increment, float* var) {
	WidgetCursorHandling();

	if (ticks < 1)
		ticks = 1;

	const int startX = cursorX;
	const int startY = cursorY;

	if (latestInputType == InputType_MOUSE) {
		
		const bool isOnSliderArea =
		latestMouseInput->col >= startX && latestMouseInput->col <= startX + ticks &&
		latestMouseInput->row == startY;

		if (latestMouseInput->action == MouseAction_LEFTPRESS && isOnSliderArea)
			SetActiveItem(ItemType_Slider, sliderID);

		if (activeItemType == ItemType_Slider &&
			activeItemID == sliderID) {
			if (latestMouseInput->action == MouseAction_LEFTDRAG || latestMouseInput->action == MouseAction_LEFTPRESS) {

				float newVal = minVal + increment * (latestMouseInput->col - startX);
				float maxVal = minVal + (increment * ticks);

				// Clamp to value limits.
				*var = newVal > maxVal ? maxVal : newVal < minVal ? minVal : newVal;
			}
		}
	}

	PushToCanvas(startX, startY, '[');

	for (int i = 0; i < ticks; i++)
		PushToCanvas(startX + 1 + i, startY, *var <= minVal + (increment * i) ? '-' : '=');

	PushToCanvas(startX + ticks + 1, startY, ']');

	sameLineX = startX + ticks + 2;
	sameLineY = startY;

	cursorX = startX;
	cursorY++;
}

bool RVMT::InputText(const char* fieldID, char* val, unsigned int maxStrSize, int width) {
	WidgetCursorHandling();

	const char* customCharset = nullptr;
	const char* emptyPlaceholder = "...";
	bool censorOutput = false;

	for (auto &prop : PushedProperties) 
		switch (prop.property) {
			case WidgetProp_InputText_Charset:
				customCharset = prop.strVal;
				break;

			case WidgetProp_InputText_Placeholder:
				emptyPlaceholder = prop.strVal;
				break;

			case WidgetProp_InputText_Censor:
				censorOutput = true;
				break;

			default:
				break;
		}

	bool rvalue = false;

	if (latestInputType == InputType_MOUSE  &&
		latestMouseInput->col >= cursorX && latestMouseInput->col <= cursorX + width + 1 &&
		latestMouseInput->row >= cursorY && latestMouseInput->row < cursorY + 3) {
		rvalue = true; // clicked

		SetActiveItem(ItemType_InputText, fieldID);
	}

	const bool thisFieldIsActive = (
		activeItemType == ItemType_InputText &&
		strEquals(activeItemID, fieldID)
	);

	int inputLength = strLength(val);

	if (thisFieldIsActive) 
		for (auto& keypress : KEYPRESSES)
			if (strEquals(keypress.field, activeItemID)) {
				const char KEY = keypress.key;

				KEYPRESSES.erase(KEYPRESSES.begin());

				// Escape / BEL
				if (KEY == 27 || KEY == 7) {
					ResetActiveItem();
					break;
				}

				// Delete / Backspace
				if (KEY == 127 || KEY == 8) {
					if (inputLength != 0) // Empty field
						val[--inputLength] = 0;
					continue;
				}

				// Look for custom charset.
				if (customCharset != nullptr &&
					!strContains(customCharset, KEY)) 
					continue;
				
				// Append pressed key to the field's text.
				else if (inputLength < maxStrSize) {
					val[inputLength] = KEY;
					val[++inputLength] = 0;
				}
			}
	
	DrawBox(cursorX, cursorY, width, 1);
	cursorX++;
	cursorY++;

	// Unfocused and empty.
	if (!thisFieldIsActive &&
		inputLength == 0)
		DrawString(cursorX, cursorY, emptyPlaceholder);

	else if (inputLength >= width) {
		if (censorOutput)
			for (int i = 0; i < width - 1; i++) 
				PushToCanvas(cursorX + i, cursorY, '*');
		else
			DrawString(cursorX, cursorY, &val[inputLength - width + 1]);

		if (thisFieldIsActive)
			PushToCanvas(cursorX + width - 1, cursorY, L'\u2588');
	}

	else {
		if (censorOutput)
			for (int i = 0; i < inputLength; i++) 
				PushToCanvas(cursorX + i, cursorY, '*');

		else
			DrawString(cursorX, cursorY, val);

		if (thisFieldIsActive)
			PushToCanvas(cursorX + inputLength, cursorY, L'\u2588');
	}

	// Handle cursor and SameLine
	sameLineX = cursorX + width + 1;
	sameLineY = cursorY - 1;

	cursorY += 2;
	cursorX--;

	PushedProperties.clear(); 
	return 0;
}

// !=== Drawing ===!
// === RVMT::DrawString
// === RVMT::DrawBox
// === RVMT::DrawHLine
// === RVMT::DrawVLine
// === RVMT::SetCursorX
// === RVMT::SetCursorY
// === RVMT::SameLine

void RVMT::DrawString(unsigned int x, unsigned int y, const char* str) {

	const int startX = x;

	for (int i = 0; i < 4096; i++) {
		if (str[i] == 0) 
			break;

		if (str[i] == 10) { // Newline
			x = startX;
			y++;
			continue;
		}
		PushToCanvas(x++, y, str[i]);
	}
}

void RVMT::DrawBox(int x, int y, int width, int height) {
	// Set box style according to BoxStyle_Current
	short TLC, TRC, BLC, BRC;

	switch (BoxStyle_Current) {
		case BoxStyle_Simple:
			TLC = 9484, TRC = 9488,
			BLC = 9492, BRC = 9496;
			break;

		case BoxStyle_Bold:
			TLC = 9487, TRC = 9491,
			BLC = 9495, BRC = 9499;
			break;

		case BoxStyle_DoubleLine:
			TLC = 9556, TRC = 9559,
			BLC = 9562, BRC = 9565;
			break;

		case BoxStyle_Round:
			TLC = 9581, TRC = 9582,
			BLC = 9584, BRC = 9583;
			break;
	}

	// Push corners
	PushToCanvas(x, y, TLC);
	PushToCanvas(x + width + 1, y, TRC);
	PushToCanvas(x, y + height + 1, BLC);
	PushToCanvas(x + width + 1, y + height + 1, BRC);

	// Push borders
	DrawHLine(x + 1, y, width); // Top
	DrawHLine(x + 1, y + height + 1, width); // Bottom

	DrawVLine(x, y + 1, height); // Left
	DrawVLine(x + width + 1, y + 1, height); // Right
}

void RVMT::DrawHLine(const int x, const int y, const unsigned short width) {
	wchar_t lineCh;

	switch (BoxStyle_Current) {
		case BoxStyle_Simple: lineCh = 9472; break;
		case BoxStyle_Bold: lineCh = 9473; break;
		case BoxStyle_DoubleLine: lineCh = 9552; break;
		case BoxStyle_Round: lineCh = 9472; break;
	}

	for (unsigned short i = 0; i < width; i++) 
		PushToCanvas(x + i, y, lineCh);
}

void RVMT::DrawVLine(const int x, const int y, const unsigned short height) {
	short lineCh;

	switch (BoxStyle_Current) {
		case BoxStyle_Simple: lineCh = 9474; break;
		case BoxStyle_Bold: lineCh = 9475; break;
		case BoxStyle_DoubleLine: lineCh = 9553; break;
		case BoxStyle_Round: lineCh = 9474; break;
	}

	for (unsigned short i = 0; i < height; i++) 
		PushToCanvas(x, y + i, lineCh);
}

void RVMT::SetCursorX(NewCursorPos mode, int value) {
	InternalSetCursor('x', mode, value);
}

void RVMT::SetCursorY(NewCursorPos mode, int value) {
	InternalSetCursor('y', mode, value);
}

void RVMT::SameLine() {
	sameLineCalled = true;

	if (!sameLinedPreviousItem)
		sameLineXRevert = cursorX,
		sameLineYRevert = cursorY;

	cursorX = sameLineX;
	cursorY = sameLineY;
}

// !=== Misc ===!
// === Input threads
// === RVMT::PushPropertyForNextItem
// === RVMT::GetActiveItemType
// === RVMT::GetActiveItemID
// === RVMT::GetColCount
// === RVMT::GetRowCount
// === RVMT::GetBoxStyle
// === RVMT::SetBoxStyle
// === RVMT::SetTerminalTitle
// === RVMT::WaitForNewInput
// === RVMT::BeginFrame
// === RVMT::Render
// === RVMT::Start
// === RVMT::Stop

struct termios _termios;
std::thread inputsThread;

bool stopCalled = false;

void inputsThreadFunc() {
	char input;
	char lastInput;
	
	while (!stopCalled) {
		std::chrono::high_resolution_clock::time_point idleTime = std::chrono::high_resolution_clock::now();
		lastInput = input;
		input = std::cin.get();

		if (input == 27)
			continue;

		if (input == 91 && lastInput == 27 && CHRONOELAPSEDMS(idleTime) < 5) {
			latestInputType = InputType_MOUSE;
			std::cin.get();
			MouseAction action;

			switch (std::cin.get()) {
				case 32: action = MouseAction_LEFTPRESS;	break;
				case 33: action = MouseAction_MIDDLEPRESS;	break;
				case 34: action = MouseAction_RIGHTPRESS;	break;
				case 35: action = MouseAction_RELEASE;		break;
				case 64: action = MouseAction_LEFTDRAG;		break;
				case 65: action = MouseAction_MIDDLEDRAG;	break;
				case 66: action = MouseAction_RIGHTDRAG;	break;
				case 67: action = MouseAction_NONE;			break;
				case 96: action = MouseAction_SCROLLUP;		break;
				case 97: action = MouseAction_SCROLLDOWN;	break;
				default: action = MouseAction_NONE; 		break;
			};
			unsigned char x = std::cin.get() - 33;
			unsigned char y = std::cin.get() - 33;

			if ((activeItemType != ItemType_Slider && action == MouseAction_LEFTDRAG) ||
				(activeItemType == ItemType_InputText && action == MouseAction_NONE) ||
				(activeItemType == ItemType_None && action == MouseAction_NONE))
				continue;

			mouseInputs.push_back({x, y, action});
		}

		else if (activeItemType == ItemType_InputText)
			KEYPRESSES.push_back({input, activeItemID});
	}
}

void RVMT::PushPropertyForNextItem(const WidgetProp property, const int value) {
	customProperty newProp;
	newProp.property = property;
	newProp.intVal = value;

	PushedProperties.push_back(newProp);
};

void RVMT::PushPropertyForNextItem(const WidgetProp property, const char* value) {
	customProperty newProp;
	newProp.property = property;
	newProp.strVal = value;

	PushedProperties.push_back(newProp);
};

void RVMT::PushPropertyForNextItem(const WidgetProp property) {
	customProperty newProp;
	newProp.property = property;

	PushedProperties.push_back(newProp);
};

ItemType RVMT::GetActiveItemType() {
	return activeItemType;
}

const char* RVMT::GetActiveItemID() {
	return activeItemID;
}

unsigned short RVMT::GetColCount() {
	return colCount;
}

unsigned short RVMT::GetRowCount() {
	return rowCount;
}

BoxStyle RVMT::GetBoxStyle() {
	return BoxStyle_Current;
}

void RVMT::SetBoxStyle(const BoxStyle newStyle) {
	BoxStyle_Current = newStyle;
}

void RVMT::SetTerminalTitle(const char* str) {
	std::wcout << "\033]2;" << str << "\007";
}

void RVMT::WaitForNewInput() {
	while (mouseInputs.size() == 0 && KEYPRESSES.size() == 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	if (mouseInputs.size() > 0) {
		mouseInputs.erase(mouseInputs.begin());
		latestMouseInput = &mouseInputs[0];
	}
}

void RVMT::BeginFrame() {
	struct winsize terminalSize;
	ioctl(1, TIOCGWINSZ, &terminalSize);

	colCount = terminalSize.ws_col;
	rowCount = terminalSize.ws_row;

	// Populate canvas.
	for (int y = 0; y < rowCount; y++)
		canvas.push_back(std::wstring(colCount, ' '));

	cursorX = 0;
	cursorY = 0;
	sameLineX = 0;
	sameLineY = 0;
	sameLineXRevert = 0;
	sameLineYRevert = 0;

	if (latestInputType == InputType_MOUSE &&
		latestMouseInput->action == MouseAction_RELEASE)
		ResetActiveItem();
}

void RVMT::Render() {
	std::wostringstream preScreen{L""};

	// Store into prescreen to print everything at once.
	for (int i = 0; i < canvas.size(); i++)
		preScreen << canvas[i];
	canvas.clear();

	// Clear screen
	std::wcout << "\033[H" << preScreen.str(); std::wcout.flush();
}

void RVMT::Start() {
	// Set locale to print unicodes correctly.
	std::locale::global(std::locale(""));

	canvas.clear();

	// Turn off canonical mode and input echoing.
	tcgetattr(0, &_termios);
	_termios.c_lflag = _termios.c_lflag ^ ICANON;
	_termios.c_lflag = _termios.c_lflag ^ ECHO;
	tcsetattr(0, 0, &_termios);

	inputsThread = std::thread(&inputsThreadFunc);

	// Hide cursor position and start capturing mouse events.
	std::wcout << "\033[?25l\033[?1003h"; std::wcout.flush();
}

void RVMT::Stop() {
	stopCalled = true;

	// Restore terminal settings
	std::wcout << "\033c Press any key to exit...\n";
	inputsThread.join();

	_termios.c_lflag = _termios.c_lflag ^ ICANON;
	_termios.c_lflag = _termios.c_lflag ^ ECHO;
	tcsetattr(0, 0, &_termios);
}