#include "rvmt.hpp"

#include <cmath>
#include <iostream>
#include <cstdarg>
#include <sstream>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>

bool strEquals(const char* first, const char* second) {
    for (int i = 0; i < 4096; i++) {
        if (first[i] == 0 && second[i] == 0)
            return true;

        if (first[i] != second[i])
            return false;
    }
    return false;
}

int strLength(const char* str) {
    if (str[0] == 0)
        return 0;

    for (int i = 0; i < 4096; i++) 
        if (str[i] == 0)
            return i;
    
    return 4096;
}

unsigned int strCount(const char* str, char ch) {
    unsigned int rvalue = 0;

    for (int i = 0; i < 4096; i++) {
        if (str[i] == 0)
            break;

        if (str[i] == ch)
            rvalue++;
    }
    return rvalue;
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

// Start extern variables.
    std::vector<RVMT::internal::drawableElement> RVMT::internal::drawList{0};
    std::vector<std::wstring> RVMT::internal::canvas{L""};
    std::wostringstream RVMT::internal::preScreen{L""};

    unsigned short RVMT::internal::rowCount = 0;
    unsigned short RVMT::internal::colCount = 0;

    int RVMT::internal::NEWINPUT_MOUSEWINX = false;
    int RVMT::internal::NEWINPUT_MOUSEWINY = false;
    int RVMT::internal::NEWINPUT_MOUSEROW = false;
    int RVMT::internal::NEWINPUT_MOUSECOL = false;
    bool RVMT::internal::PREVINPUT_MOUSE1HELD = false;
    bool RVMT::internal::NEWINPUT_MOUSE1HELD = false;

    int RVMT::internal::NEWINPUT_TERMX = false;
    int RVMT::internal::NEWINPUT_TERMY = false;
    unsigned int RVMT::internal::NEWINPUT_TERMWIDTH = false;
    unsigned int RVMT::internal::PREVINPUT_TERMWIDTH = false;
    unsigned int RVMT::internal::NEWINPUT_TERMHEIGHT = false;
    unsigned int RVMT::internal::PREVINPUT_TERMHEIGHT = false;

    int RVMT::internal::NEWINPUT_CELLWIDTH = false;

    bool RVMT::internal::sameLineCalled = false;
    bool RVMT::internal::sameLinedPreviousItem = false;

    int RVMT::internal::sameLineX = 0;
    int RVMT::internal::sameLineXRevert = 0;
    int RVMT::internal::sameLineY = 0;
    int RVMT::internal::sameLineYRevert = 0;

    Display* RVMT::internal::rootDisplay = nullptr;
    Window RVMT::internal::rootWindow = 0;
    Window RVMT::internal::termX11Win = 0;

    RVMT::internal::ItemType_ RVMT::internal::activeItemType = ItemType_None;
    const char* RVMT::internal::activeItemID = "none";

    bool RVMT::internal::startCalled = false;
    bool RVMT::internal::stopCalled = false;

    std::vector<RVMT::internal::keyPress> RVMT::internal::KEYPRESSES;

    int RVMT::internal::cursorX = 0;
    int RVMT::internal::cursorY = 0;

    std::vector<bool> RVMT::renderRequests{1}; // Start with a single render request

    int BoxStyle_Current = BoxStyle_Round;
    
using namespace RVMT;
using namespace RVMT::internal;

struct CustomProperty {
    WidgetProp property = WidgetProp_NULL_RVMT_WIDGET_PROPERTY;
    int intVal = 0;
    const char* strVal = "none";
};

CustomProperty _DefaultCustomProperty;

std::vector<CustomProperty> pushedProperties;

int _NULLINT = 0;
unsigned int _NULLUINT = 0;
Window _NULLX11WINDOW = 0;

bool manuallyModdedCurrentCursor = false;

void requestNewRender() {
    renderRequests.push_back(1);
};

void resetActiveItem() {
    activeItemType = ItemType_None;
    activeItemID = "none";
}

void preWidgetDrawCursorHandling() {
    if (sameLineCalled)
        cursorX = sameLineX,
        cursorY = sameLineY,
        sameLineCalled = false,
        sameLinedPreviousItem = true;

    else if (!sameLineCalled && sameLinedPreviousItem)
        cursorX = sameLineXRevert,
        cursorY = sameLineYRevert,
        sameLinedPreviousItem = false;
}

// !=== Widgets ===!
// === RVMT::Text
// === RVMT::Checkbox
// === RVMT::Button
// === RVMT::Slider
// === RVMT::InputText

void RVMT::Text(const char* val, ...) {
    preWidgetDrawCursorHandling();
        
    char buffer[1024];

    va_list args;
    va_start(args, val);
    const int textLength = vsnprintf(buffer, 1024, val, args);
    va_end(args);

    DrawText(cursorX, cursorY, buffer);

    sameLineX = cursorX + textLength;
    sameLineY = cursorY;

    cursorY += 1 + strCount(val, 10); // Count newlines
}

bool RVMT::Checkbox(const char* trueText, const char* falseText, bool* val) {
    preWidgetDrawCursorHandling();

    const int startX = cursorX;
    const int startY = cursorY;

    // Print text
    const char* ptr = *val ? &trueText[0] : &falseText[0];
    unsigned short textWidth = 0;
    for (unsigned short i = 0; i < 32767; i++) {
        if (ptr[i] == 0) {
            textWidth = i;
            break;
        }
        drawList.push_back({cursorX++, cursorY, ptr[i]});
    }

    // Handle cursor and SameLine.
    sameLineX = cursorX;
    sameLineY = cursorY;

    cursorX = startX;
    cursorY++;

    // Handle return value
    if (!PREVINPUT_MOUSE1HELD && NEWINPUT_MOUSE1HELD &&
        NEWINPUT_MOUSECOL >= startX && NEWINPUT_MOUSECOL < startX + textWidth &&
        NEWINPUT_MOUSEROW == startY) {
        
        resetActiveItem();
        *val = !*val;
        activeItemType = ItemType_Checkbox;
        return true;
    }

    return false;
}

bool RVMT::Button(const char* str, ...) {

    bool drawTextOnly = false;
    for (auto &prop : pushedProperties)
        if (prop.property == WidgetProp_Button_TextOnly)
            drawTextOnly = true;

    preWidgetDrawCursorHandling();

    // Parse argument list
    char buffer[1024];
    va_list args;
    va_start(args, str);
    const auto textLength = vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    const int startX = cursorX;
    const int startY = cursorY;
    const unsigned int buttonHeight = drawTextOnly ? 1 : 3;
    const unsigned int buttonWidth = drawTextOnly ? textLength : textLength + 2;

    if (!drawTextOnly) {
        DrawBox(cursorX, cursorY, textLength, 1);

        // Prepare cursor for text to be drawn at the box's middle
        cursorX++;
        cursorY++;
    }

    DrawText(cursorX, cursorY, buffer);

    cursorX = startX;
    cursorY = startY + buttonHeight;

    sameLineX = startX + buttonWidth;
    sameLineY = startY;

    pushedProperties.clear();
    // Handle return value
    if (!PREVINPUT_MOUSE1HELD && NEWINPUT_MOUSE1HELD &&
        NEWINPUT_MOUSECOL >= startX && NEWINPUT_MOUSECOL <= startX + buttonWidth &&
        NEWINPUT_MOUSEROW >= startY && NEWINPUT_MOUSEROW < startY + buttonHeight) {

        resetActiveItem();
        activeItemType = ItemType_Button;
        return true;
    }
    return false;
}

bool RVMT::Slider(const char* sliderID, int length, float minVal, float maxVal, float* var) {

    if (length < 1)
        length = 1;

    preWidgetDrawCursorHandling();


    const int x = cursorX;
    const int y = cursorY;

    const int startXPX = (x + 1) * NEWINPUT_CELLWIDTH;
    const int endXPX = (x + length + 1) * NEWINPUT_CELLWIDTH;

    bool rvalue = false; // Idle.

    // Begin interaction
    if (activeItemType != ItemType_Slider &&
        NEWINPUT_MOUSE1HELD &&
        NEWINPUT_MOUSEWINX > startXPX && NEWINPUT_MOUSEWINX < endXPX &&
        NEWINPUT_MOUSEROW == y) {

        resetActiveItem();
        rvalue = true; // Clicked
        activeItemType = ItemType_Slider;
        activeItemID = sliderID;
    }

    // Continue interaction
    if (activeItemType == ItemType_Slider &&
        strEquals(activeItemID, sliderID)) {

        if (NEWINPUT_MOUSEWINX >= endXPX) // Clamp to maxval
            *var = maxVal;

        else if (NEWINPUT_MOUSEWINX <= startXPX) // Clamp to minval
            *var = minVal;

        else
            *var = minVal + (((maxVal - minVal) / (endXPX - startXPX)) * (NEWINPUT_MOUSEWINX - startXPX));
    }

    // Prepare slider output
    std::string sliderStr(length, '-');

    const float sliderTickVal = (maxVal - minVal) / length;
    for (int i = 0; sliderTickVal * (i + 1) <= *var - minVal; i++) 
        sliderStr[i] = '=';

    // Push slider content
    drawList.push_back({x, y, '['});

    for (int i = 1; i <= length; i++) 
        drawList.push_back({x+i, y, sliderStr[i-1]});

    drawList.push_back({x+length+1, y, ']'});

    // Handle SameLine and cursor.
    sameLineX = cursorX + length + 2;
    sameLineY = cursorY;

    cursorX = x;
    cursorY++;

    return rvalue;
}

bool RVMT::InputText(const char* fieldID, char* val, unsigned int maxStrSize, int width) {

    const char* customCharset = nullptr;
    const char* idlingText = "...";
    bool censorOutput = false;

    for (auto &prop : pushedProperties) 
        switch (prop.property) {
            case WidgetProp_InputText_CustomCharset:
                customCharset = prop.strVal;
                break;

            case WidgetProp_InputText_IdleText:
                idlingText = prop.strVal;
                break;

            case WidgetProp_InputText_Censor:
                censorOutput = true;
                break;

            default:
                break;
        }
    
    preWidgetDrawCursorHandling();

    const int startX = cursorX;
    const int startY = cursorY;
    bool rvalue = false;

    if (!PREVINPUT_MOUSE1HELD && NEWINPUT_MOUSE1HELD &&
        NEWINPUT_MOUSECOL >= startX && NEWINPUT_MOUSECOL <= startX + width + 1 &&
        NEWINPUT_MOUSEROW >= startY && NEWINPUT_MOUSEROW < startY + 3) {

        resetActiveItem();
        rvalue = true; // clicked
        activeItemType = ItemType_InputText;
        activeItemID = fieldID;
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
                    activeItemID = "none";
                    activeItemType = ItemType_None;
                    break;
                }

                // Delete / Backspace
                if (KEY == 127 || KEY == 8) {
                    if (inputLength == 0) // Empty field
                        continue;

                    val[inputLength - 1] = 0;
                }

                if (customCharset != nullptr &&
                    !strContains(customCharset, KEY)) 
                    continue;
                

                else if (inputLength < maxStrSize) {
                    val[inputLength] =
                        censorOutput
                        ? '*'
                        : KEY;
                    val[++inputLength] = 0;
                }
            }
    
    DrawBox(cursorX, cursorY, width, 1);
    cursorX++;
    cursorY++;

    // Unfocused and no text written.
    if (!thisFieldIsActive &&
        inputLength == 0)
        DrawText(cursorX, cursorY, idlingText);

    else if (inputLength > width)
        DrawText(cursorX, cursorY, &val[inputLength - width]);

    else
        DrawText(cursorX, cursorY, val);

    // Handle cursor and SameLine
    sameLineX = cursorX + width + 1;
    sameLineY = cursorY - 1;

    cursorY += 2;
    cursorX--;

    pushedProperties.clear(); 
    return 0;
}

// !=== Drawing ===!
// === RVMT::DrawText
// === RVMT::DrawBox
// === RVMT::DrawHSeparator
// === RVMT::DrawVSeparator
// === RVMT::SetCursorX
// === RVMT::SetCursorY
// === RVMT::SameLine

void RVMT::DrawText(int x, int y, const char* val) {
    const int startX = x;

    for (int z = 0; z < 1024; z++) {
        if (val[z] == 0) {
            break;
        }

        if (val[z] == 10) { // Newline
            x = startX;
            y++;
            continue;
        }

        drawList.push_back({x++, y, val[z]});
    }
}

void RVMT::DrawBox(int x, int y, int width, int height) {
    // Note that this function does not change the cursor.
    // Rather doing this than doing "+1" every time these are called.
    height++;
    width++;

    // Set box style according to BoxStyle_Current
    short TLC, TRC, vBorders;
    short BLC, BRC;
    short hBorders;

    switch (BoxStyle_Current) {
        case BoxStyle_Simple:
            TLC = 9484, TRC = 9488, vBorders = 9474,
            BLC = 9492, BRC = 9496,
            hBorders = 9472;
            break;

        case BoxStyle_Bold:
            TLC = 9487, TRC = 9491, vBorders = 9475,
            BLC = 9495, BRC = 9499,
            hBorders = 9473;
            break;

        case BoxStyle_DoubleLine:
            TLC = 9556, TRC = 9559, vBorders = 9553,
            BLC = 9562, BRC = 9565,
            hBorders = 9552;
            break;

        case BoxStyle_Round:
            TLC = 9581, TRC = 9582, vBorders = 9474,
            BLC = 9584, BRC = 9583,
            hBorders = 9472;
            break;
    }

    // Push corners
    drawList.push_back({x, y, TLC});
    drawList.push_back({x + width, y, TRC});

    drawList.push_back({x, y + height, BLC});
    drawList.push_back({x + width, y + height, BRC});

    // Push borders
    for (unsigned short i = 1; i < width ; i++) // Top
        drawList.push_back({x + i, y, hBorders});

    for (unsigned short i = 1; i < width; i++) // Bottom
        drawList.push_back({x + i, y + height, hBorders});

    for (unsigned short i = 1; i < height; i++) // Left
        drawList.push_back({x, y + i, vBorders});

    for (unsigned short i = 1; i < height; i++) // Right
        drawList.push_back({x + width, y + i, vBorders});

}

void RVMT::DrawHSeparator(int x, int y, int width) {
    wchar_t leftLimit, middle, rightLimit;

    switch (BoxStyle_Current) {
        case BoxStyle_Simple:
            leftLimit = 9500, middle = 9472, rightLimit = 9508;
            break;

        case BoxStyle_Bold:
            leftLimit = 9507, middle = 9473, rightLimit = 9515;
            break;

        case BoxStyle_DoubleLine:
            leftLimit = 9568, middle = 9552, rightLimit = 9571;
            break;

        case BoxStyle_Round: // Same as simple
            leftLimit = 9500, middle = 9472, rightLimit = 9508;
            break;
    }

    drawList.push_back({x, y, leftLimit});

    for (int i = 1; i <= width; i++)
        drawList.push_back({x+i, y, middle});

    drawList.push_back({x + width + 1, y, rightLimit});
    
}

void RVMT::DrawVSeparator(int x, int y, int height) {
    wchar_t topLimit, middle, bottomLimit;

    switch (BoxStyle_Current) {
        case BoxStyle_Simple:
            topLimit = 9516, middle = 9474, bottomLimit = 9524;
            break;

        case BoxStyle_Bold:
            topLimit = 9523, middle = 9475, bottomLimit = 9531;
            break;

        case BoxStyle_DoubleLine:
            topLimit = 9574, middle = 9553, bottomLimit = 9577;
            break;

        case BoxStyle_Round: // Same as simple
            topLimit = 9516, middle = 9474, bottomLimit = 9524;
            break;
    }

    drawList.push_back({x, y, topLimit});

    for (int i = 1; i <= height; i++)
        drawList.push_back({x, y + i, middle});

    drawList.push_back({x, y + height + 1, bottomLimit});
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

void RVMT::PushPropertyForNextItem(WidgetProp property, int value) {
    CustomProperty newProp;

    newProp.property = property;
    newProp.intVal = value;

    pushedProperties.push_back(newProp);
};

void RVMT::PushPropertyForNextItem(WidgetProp property, const char* value) {
    CustomProperty newProp;

    newProp.property = property;
    newProp.strVal = value;

    pushedProperties.push_back(newProp);
};

void RVMT::PushPropertyForNextItem(WidgetProp property) {
    CustomProperty newProp;

    newProp.property = property;

    pushedProperties.push_back(newProp);
};


// !=== Internal ===!
// === Input threads
// === RVMT::internal::InternalSetCursor
// === RVMT::Render
// === RVMT::Start
// === RVMT::Stop

struct termios _termios;
std::thread mouseInputThread;
std::thread kbInputsThread;

void mouseInputThreadFunc() {

    while (!stopCalled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // In some cases, such as in i3-wm:
        // XGetInputFocus somehow does not return the same as XQueryPointer.
        // XGetInputFocus returns the "child"
        // XQueryPointer returns the "parent"

        Window currentWindow;
        XQueryPointer(rootDisplay, rootWindow, &_NULLX11WINDOW, &currentWindow,
            &_NULLINT, &_NULLINT, &_NULLINT, &_NULLINT, &_NULLUINT);

        if (currentWindow != termX11Win &&
            activeItemType != ItemType_Slider)
            continue;
            
        // === Get borders thickness.
        // There are two ways of getting it.
        // 1) Using the XGetGeometry function that literally retrieves the borders' size.
        unsigned int topBorder;
        unsigned int leftBorder;

        PREVINPUT_TERMWIDTH = NEWINPUT_TERMWIDTH;
        PREVINPUT_TERMHEIGHT = NEWINPUT_TERMHEIGHT;

        XGetGeometry(rootDisplay, termX11Win, &_NULLX11WINDOW, &NEWINPUT_TERMX, &NEWINPUT_TERMY, &NEWINPUT_TERMWIDTH, &NEWINPUT_TERMHEIGHT, &leftBorder, &topBorder);

        // 2) Getting the "inner window's" x and y. Used only if the first one doesn't work.
        if (topBorder == 0 && leftBorder == 0) {
            unsigned long innerWindow;
            XGetInputFocus(rootDisplay, &innerWindow, &_NULLINT);

            XWindowAttributes windowAttributes;
            XGetWindowAttributes(rootDisplay, innerWindow, &windowAttributes);

            if (windowAttributes.y >= 0)
                topBorder = windowAttributes.y;

            if (windowAttributes.x >= 0)
                leftBorder = windowAttributes.x;
        }

        NEWINPUT_TERMWIDTH -= (leftBorder*2);
        NEWINPUT_TERMHEIGHT -= topBorder;

        // === Get mouse values.
        unsigned int mouseMask;
        int mouseXPos;
        int mouseYPos;
        XQueryPointer(rootDisplay, rootWindow, &_NULLX11WINDOW, &_NULLX11WINDOW,
                    &_NULLINT, &_NULLINT, &mouseXPos, &mouseYPos, &mouseMask);

        NEWINPUT_MOUSEWINX = mouseXPos - NEWINPUT_TERMX - leftBorder;
        NEWINPUT_MOUSEWINY = mouseYPos - NEWINPUT_TERMY - topBorder;

        PREVINPUT_MOUSE1HELD = NEWINPUT_MOUSE1HELD;
        NEWINPUT_MOUSE1HELD = mouseMask & Button1Mask;

        // "> 0"'s here to prevent floating point exceptions.
        if (colCount > 0 && rowCount > 0) {
            NEWINPUT_CELLWIDTH = ((float)NEWINPUT_TERMWIDTH / (float)colCount);
        
            if (NEWINPUT_TERMWIDTH > 0)
                NEWINPUT_MOUSECOL = std::floor((float)NEWINPUT_MOUSEWINX / ((float)NEWINPUT_TERMWIDTH / (float)colCount));
            
            if (NEWINPUT_TERMHEIGHT > 0) 
                NEWINPUT_MOUSEROW = std::floor((float)NEWINPUT_MOUSEWINY / ((float)NEWINPUT_TERMHEIGHT / (float)rowCount));
        }

        if (NEWINPUT_MOUSE1HELD ||
            (PREVINPUT_MOUSE1HELD && !NEWINPUT_MOUSE1HELD) ||
            PREVINPUT_TERMWIDTH != NEWINPUT_TERMWIDTH ||
            PREVINPUT_TERMHEIGHT != NEWINPUT_TERMHEIGHT)
            renderRequests.push_back(1);
    }
}

void kbInputsThreadFunc() {
    while (!stopCalled) {
        const char LATEST_KEYPRESS = std::cin.get();

        if (activeItemType == ItemType_InputText) {
            KEYPRESSES.push_back({
                LATEST_KEYPRESS,
                activeItemID
            });
            renderRequests.push_back(1);
        }
    }
}

void RVMT::internal::InternalSetCursor(char axis, NewCursorPos mode, int value) {
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

void RVMT::Render() {

    // Reset active item if idling.
    if (!NEWINPUT_MOUSE1HELD &&
        activeItemType != ItemType_None &&
        activeItemType != ItemType_InputText) {
        
        activeItemType = ItemType_None;
        activeItemID = "none";
        renderRequests.push_back(1);
    }

    struct winsize terminalSize;
    ioctl(1, TIOCGWINSZ, &terminalSize);

    rowCount = terminalSize.ws_row;
    colCount = terminalSize.ws_col;

    // Populate canvas.
    for (int y = 0; y < rowCount; y++)
        canvas.push_back(std::wstring(colCount, ' '));

    // Push drawlist's elements into the canvas.
    for (auto &elem : drawList) {
        if (elem.y >= canvas.size())
            continue;
        
        if (elem.x >= canvas[elem.y].length())
            continue;
        canvas[elem.y][elem.x] = elem.ch;
    }
    drawList.clear();

    // Store into prescreen to print everything at once.
    for (int i = 0; i < canvas.size(); i++)
        preScreen << canvas[i];
    canvas.clear();

    // Clear screen
    std::wcout << "\033[H\033[J";

    std::wcout << preScreen.str(); std::wcout.flush();
    preScreen.str(L"");

    cursorX = 0;
    cursorY = 0;
}

void RVMT::Start() {
    // Set locale to print unicodes correctly.
    std::locale::global(std::locale(""));

    // Clear non-initialized vars from any possible trash data
    drawList.clear();
    canvas.clear();
    preScreen.str(L"");

    rootDisplay = XOpenDisplay(NULL);
    rootWindow = DefaultRootWindow(rootDisplay);

    XQueryPointer(
        rootDisplay, rootWindow, &_NULLX11WINDOW, &termX11Win,
        &_NULLINT, &_NULLINT, &_NULLINT, &_NULLINT, &_NULLUINT);


    tcgetattr(0, &_termios);
    // Turn off canonical mode and input echoing for keyboard inputs.
    _termios.c_lflag = _termios.c_lflag ^ ICANON;
    _termios.c_lflag = _termios.c_lflag ^ ECHO;
    tcsetattr(0, 0, &_termios);

    // Start input threads.
    mouseInputThread = std::thread(&mouseInputThreadFunc);
    kbInputsThread = std::thread(&kbInputsThreadFunc);

    startCalled = true;
}

void RVMT::Stop() {
    stopCalled = true;

    // Wait for input threads to finish.
    std::wcout << "\nPress any key to exit...\n";
    mouseInputThread.join();
    kbInputsThread.join();

    // Restore terminal to the normal state.
    _termios.c_lflag = _termios.c_lflag ^ ICANON;
    _termios.c_lflag = _termios.c_lflag ^ ECHO;
    tcsetattr(0, 0, &_termios);
    
    XCloseDisplay(rootDisplay);
}