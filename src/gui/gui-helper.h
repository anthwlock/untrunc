#ifndef GUIHELPER_H
#define GUIHELPER_H

#include <ui.h>
#include <utility>
#include <vector>

class GuiHelper {
public:
	GuiHelper() = delete;
	enum types {
		window,
		box,
		tab,
		group,
		form,
		grid,
	};
	struct Thing {
		Thing(void* ptr, int type);
		void* ptr;
		int type;
	};

	static void setSpaced(bool spaced);
	static void setDisabled(bool setDisabled);
	static uiWindow* newWindow(const char* title, int width, int height,
	                           int hasMenubar);
	static uiBox* newHorizontalBox(void);
	static uiBox* newVerticalBox(void);
	static uiTab* newTab(void);
	static uiGroup* newGroup(const char* text);
	static uiForm* newForm(void);
	static uiGrid* newGrid(void);
	static uiButton* newButton(const char* text);
	static uiEntry* newEntry();
	static void append(void* ptr, int type);
private:
	static std::vector<Thing> elements_;
	static std::vector<void*> to_disable_;
	static void setSpacedThing(Thing& t, bool spaced);
};

#endif  // GUIHELPER_H
