#ifndef GUI_H
#define GUI_H

#include <ui.h>
#include <sstream>
#include <thread>

#include "gui-helper.h"
#include "../mp4.h"

class Gui : public GuiHelper {
public:
	Gui() = delete;
	static void init();
	static void run();

	struct Repair {
		static uiEntry* entry_ok_;
		static uiEntry* entry_bad_;
		static uiMultilineEntry* text_entry_;
		static uiLabel* label_status_;
		static uiProgressBar* progressbar_;
		static void onProgress(int percentage);
		static void onStatus(const std::string& status);
	};

	struct Analyze {
		static uiEntry* entry_ok_;
		static uiMultilineEntry* text_entry_;
		static uiProgressBar* progressbar_;
		static void onProgress(int percentage);
	};

private:
	static uiWindow* window_;

	static uiMultilineEntry* current_text_entry_;

	static void build();
	static void openFile(uiButton* b, void* data);
	static int onClosing(uiWindow* w, void* data);
	static int writeOutput(void* data);
	static uiEntry* addFileOpen(uiBox* box, const char* text);
	static void startRepair(uiButton* button, void* data);

	static uiControl* repairTab();
	static uiControl* analyzeTab();
	static uiControl* settingsTab();
	static uiControl* aboutTab();

	static void msgBox(const char* text);
	static void msgBoxError(const std::string& text, bool async=false);
	static void msgBoxErrorSync(const std::string& text);
	static void msgBoxErrorQueued(const std::string* s);
	static void printToEntry(const char* text);

	static std::thread* thread_;

	static std::stringstream buffer_;
};

#endif  // GUI_H
