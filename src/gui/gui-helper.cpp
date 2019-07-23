#include "gui-helper.h"

using namespace std;

vector<GuiHelper::Thing> GuiHelper::elements_;
vector<void*> GuiHelper::to_disable_;

void GuiHelper::setSpacedThing(Thing& t, bool spaced) {
	size_t j, n;
	void* p = t.ptr;

	switch (t.type) {
		case window:
			uiWindowSetMargined(uiWindow(p), spaced);
			break;
		case box:
			uiBoxSetPadded(uiBox(p), spaced);
			break;
		case tab:
			n = uiTabNumPages(uiTab(p));
			for (j = 0; j < n; j++)
				uiTabSetMargined(uiTab(p), j, spaced);
			break;
		case group:
			uiGroupSetMargined(uiGroup(p), spaced);
			break;
		case form:
			uiFormSetPadded(uiForm(p), spaced);
			break;
		case grid:
			uiGridSetPadded(uiGrid(p), spaced);
			break;
	}
}

void GuiHelper::setSpaced(bool spaced) {
	for (Thing& t : elements_)
		setSpacedThing(t, spaced);
}

void GuiHelper::setDisabled(bool disable) {
	if (disable)
		for (void* c : to_disable_)
			uiControlDisable(uiControl(c));
	else
		for (void* c : to_disable_)
			uiControlEnable(uiControl(c));
}

uiWindow* GuiHelper::newWindow(const char* title, int width, int height,
                               int hasMenubar) {
	auto w = uiNewWindow(title, width, height, hasMenubar);
	append(w, window);
	return w;
}

uiBox* GuiHelper::newHorizontalBox(void) {
	auto b = uiNewHorizontalBox();
	append(b, box);
	return b;
}

uiBox* GuiHelper::newVerticalBox(void) {
	auto b = uiNewVerticalBox();
	append(b, box);
	return b;
}

uiTab* GuiHelper::newTab(void) {
	auto t = uiNewTab();
	append(t, tab);
//	GuiHelper::to_disable_.emplace_back(t);
	return t;
}

uiGroup* GuiHelper::newGroup(const char* text) {
	auto g = uiNewGroup(text);
	append(g, group);
	return g;
}

uiForm* GuiHelper::newForm(void) {
	auto f = uiNewForm();
	append(f, form);
	return f;
}

uiGrid* GuiHelper::newGrid(void) {
	auto g = uiNewGrid();
	append(g, grid);
	return g;
}

uiButton* GuiHelper::newButton(const char* text) {
	auto b = uiNewButton(text);
	GuiHelper::to_disable_.emplace_back(b);
	return b;
}

uiEntry* GuiHelper::newEntry(void) {
	auto e = uiNewEntry();
	GuiHelper::to_disable_.emplace_back(e);
	return e;
}

void GuiHelper::append(void* ptr, int type) {
	elements_.emplace_back(ptr, type);

	// TODO: trigger update/rerender correctly
	#ifdef __MINGW32__
	setSpaced(true);
	#endif
}


GuiHelper::Thing::Thing(void* ptr, int type) : ptr(ptr), type(type) {}
