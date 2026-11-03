#ifndef FILE_DROP_H
#define FILE_DROP_H

#include <functional>
#include <map>
#include <string>

#ifdef _WIN32
#include <ui.h>

// Windows-only OLE drag-and-drop of files onto registered picker controls.
class FileDrop {
public:
	using Handler = std::function<void(const std::string& file)>;

	static void enable(uiWindow* window);
	static void registerPicker(uiControl* control, uiEntry* target);
};

#endif  // _WIN32
#endif  // FILE_DROP_H