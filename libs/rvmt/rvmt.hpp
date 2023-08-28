#ifndef RVMT_HPP
#define RVMT_HPP

enum BoxStyle {
	BoxStyle_Simple			= 0,
	BoxStyle_Bold			= 1,
	BoxStyle_DoubleLine		= 2,
	BoxStyle_Round			= 3
};

enum NewCursorPos {
	NewCursorPos_ADD        = 0,
	NewCursorPos_SUBTRACT   = 1,
	NewCursorPos_ABSOLUTE   = 2
};

enum WidgetProp {
	WidgetProp_NULL_RVMT_WIDGET_PROPERTY	= 0,
	WidgetProp_InputText_Charset			= 1,
	WidgetProp_InputText_Placeholder		= 2,
	WidgetProp_InputText_Censor				= 3,
	WidgetProp_Button_TextOnly				= 4		
};

enum ItemType {
	ItemType_None		= 0,
	ItemType_Slider		= 1,
	ItemType_Button		= 2,
	ItemType_Checkbox	= 3,
	ItemType_InputText	= 4
};

namespace RVMT {

	// !=== Widgets ===!
	// === Text
	// === Checkbox
	// === Button
	// === Slider
	// === InputText

	// Prints text.
	extern void Text(const char* val, ...);

	// Print a checkbox.
	// When val is true, it will print trueText.
	// When val is false, it will print falseText.
	// Returns true if the text is clicked.
	extern bool Checkbox(const char* trueText, const char* falseText, bool* val);

	// Print a button.
	// Uses "BoxStyle_Current" for its style.
	// Returns true if the box is clicked.
	extern bool Button(const char* text, ...);

	// Print a draggable slider.
	extern void Slider(const char* sliderID, unsigned short ticks, float minVal, float increment, float* var);

	// Print a text input field.
	// Returns true if it's clicked
	extern bool InputText(const char* fieldID, char* val, unsigned int maxStrSize, int width);

	// !=== Drawing ===!
	// === DrawString
	// === DrawBox
	// === DrawHLine
	// === DrawVLine
	// === SetCursorX
	// === SetCursorY
	// === SameLine
	// Functions prefixed by "Draw" do not modify the cursor.

	// Draws a string without modifying the cursor. Max string length: 4096.
	// String should be null-terminated.
	extern void DrawString(unsigned int x, unsigned int y, const char* val);

	// Draws a square.
	// Gets style from BoxStyle_Current.
	extern void DrawBox(int x, int y, int width, int height);

	// Draw a horizontal line.
	extern void DrawHLine(const int x, const int y, const unsigned short width);

	// Draw a vertical line.
	extern void DrawVLine(const int x, const int y, const unsigned short height);

	// Moves X Cursor.
	extern void SetCursorX(const NewCursorPos mode, const int value);

	// Moves Y Cursor.
	extern void SetCursorY(const NewCursorPos mode, const int value);

	// Move cursor to the previous element's right for the next item.
	extern void SameLine();

	// !=== Misc ===!
	// === PushPropertyForNextItem
	// === GetActiveItemType
	// === GetActiveItemID
	// === GetColCount
	// === GetRowCount
	// === GetBoxStyle
	// === SetBoxStyle
	// === SetTerminalTitle
	// === WaitForNewInput
	// === Render
	// === Start
	// === Stop

	// Push a property for the next widget.
	extern void PushPropertyForNextItem(const WidgetProp property);

	// Push a property for the next widget along with an int.
	extern void PushPropertyForNextItem(const WidgetProp property, const int value);

	// Push a property for the next widget along with a string.
	extern void PushPropertyForNextItem(const WidgetProp property, const char* value);

	// Get active Item type. (See ItemType enum)
	extern ItemType GetActiveItemType();

	// Get active Item ID. If the item has no ID, "none" will be returned.
	extern const char* GetActiveItemID();

	// Get total columns count that are being currently displayed.
	extern unsigned short GetColCount();

	// Get total rows count that are being currently displayed.
	extern unsigned short GetRowCount();
	
	// Get current Box style.
	extern BoxStyle GetBoxStyle();

	// Set a new Box style.
	extern void SetBoxStyle(const BoxStyle newStyle);

	// Set terminal's title.
	extern void SetTerminalTitle(const char* str);
	
	// Stall the calling thread until a new input is detected.
	extern void WaitForNewInput();

	// Prepare internal stuff for a new frame.
	extern void BeginFrame();

	// Render canvas' content.
	extern void Render();

	// Set up required settings in order for RVMT to work properly.
	extern void Start();

	// Stop.
	extern void Stop();
}
#endif