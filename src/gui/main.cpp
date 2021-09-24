#include <string.h>  // memset
#include <iostream>

//#include "libavutil/ffversion.h"
#include "../common.h"
#include "gui.h"

using namespace std;

int main(int argc, char* argv[]) {
	g_interactive = false;
	g_is_gui = true;

	argv_as_utf8(argc, argv);

	Gui::init();
	if (argc > 1) uiEntrySetText(Gui::Repair::entry_ok_, argv[1]);
	if (argc > 1) uiEntrySetText(Gui::Analyze::entry_ok_, argv[1]);
	if (argc > 2) uiEntrySetText(Gui::Repair::entry_bad_, argv[2]);
	Gui::run();

	return 0;
}
