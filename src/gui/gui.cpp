#include "gui.h"

#include <string.h>  // memset
#include <thread>

extern "C" {
#include "libavutil/log.h"
}

#include "../atom.h"
#include "../common.h"
#include "../mp4.h"
using namespace std;

stringstream Gui::buffer_ = stringstream();
uiWindow* Gui::window_;
uiMultilineEntry* Gui::current_text_entry_;
uiEntry* Gui::Repair::entry_ok_;
uiEntry* Gui::Repair::entry_bad_;
uiMultilineEntry* Gui::Repair::text_entry_;
uiLabel* Gui::Repair::label_status_;
uiProgressBar* Gui::Repair::progressbar_;
uiEntry* Gui::Analyze::entry_ok_;
uiMultilineEntry* Gui::Analyze::text_entry_;
uiProgressBar* Gui::Analyze::progressbar_;
thread* Gui::thread_ = nullptr;

void Gui::init() {
	uiInitOptions o;
	memset(&o, 0, sizeof(uiInitOptions));
	if (uiInit(&o) != NULL) abort();

	window_ = newWindow("untrunc-gui", 820, 640, 0);

	build();
	setSpaced(true);

	cout.rdbuf(buffer_.rdbuf());
	cerr.rdbuf(buffer_.rdbuf());
	av_log_set_callback([](void* ptr, int loglevel, const char* msg, va_list vl) {
		if (loglevel > av_log_get_level()) return;

		const uint limit = 256;
		char buffer[limit+1];
		AVClass *avc = ptr ? *(AVClass**)ptr : NULL;
		auto n = snprintf(buffer, limit, "[%s @ %p] ", avc->item_name(ptr), ptr);
		vsnprintf(buffer+n, limit-n, msg, vl);

		cout << buffer << '\n';
	});

	current_text_entry_ = Gui::Repair::text_entry_;
	uiTimer(100, Gui::writeOutput, NULL);

	uiWindowOnClosing(window_, Gui::onClosing, NULL);
}

void Gui::run() {
	uiControlShow(uiControl(window_));
	uiMain();
}

void Gui::build() {
	struct {
		const char* name;
		uiControl* (*f)(void);
	} pages[] = {
	    {"Repair", Gui::repairTab},
	    {"Settings", Gui::settingsTab},
	    {"Analyze", Gui::analyzeTab},
	    {"About", Gui::aboutTab},
	    {NULL, NULL},
    };

	auto tabs = newTab();
	for (uint i = 0; pages[i].name != NULL; i++)
		uiTabAppend(tabs, pages[i].name, (*(pages[i].f))());

	uiWindowSetChild(window_, uiControl(tabs));
}

void Gui::openFile(uiButton* b, void* data) {
	auto fn = uiOpenFile(window_);
	if (fn) {
		uiEntrySetText(uiEntry(data), fn);
		uiFreeText(fn);
	}
}

int Gui::onClosing(uiWindow* w, void* data) {
	if (thread_)  {
		thread_->join();
		delete(thread_);
	}
	uiQuit();
	return 1;
}

int Gui::writeOutput(void* data) {
	auto txt = uiNewAttributedString(buffer_.str().c_str());
	if (uiAttributedStringLen(txt)) {
		uiMultilineEntryAppend(current_text_entry_, uiAttributedStringString(txt));
		buffer_.str("");
	}
//	uiMultilineEntryAppend(current_text_entry_, buffer_.str().c_str());
	return 1;
}

uiEntry* Gui::addFileOpen(uiBox* parent_box, const char* text) {
	auto h_box = newHorizontalBox();

	auto button = newButton(text);
	auto entry = newEntry();
	uiButtonOnClicked(button, openFile, entry);
	uiBoxAppend(h_box, uiControl(button), 0);
	uiBoxAppend(h_box, uiControl(entry), 1);
	uiBoxAppend(parent_box, uiControl(h_box), 1);
	return entry;
}

uiControl* Gui::repairTab() {
	auto v_box = newVerticalBox();

	auto h_box1 = newHorizontalBox();
	Repair::entry_ok_ = addFileOpen(h_box1, "reference file");
	Repair::entry_bad_ = addFileOpen(h_box1, "truncated file");
	uiBoxAppend(v_box, uiControl(h_box1), 0);

	Repair::text_entry_ = uiNewMultilineEntry();
	uiMultilineEntrySetReadOnly(Repair::text_entry_, 1);
	uiBoxAppend(v_box, uiControl(Repair::text_entry_), 1);

	auto h_box2 = newHorizontalBox();
	Repair::label_status_ = uiNewLabel("");
	auto b_repair = newButton("Repair");
	uiButtonOnClicked(b_repair, startRepair, NULL);
	//	uiBoxAppend(v_box, uiControl(b_repair), 0);
	uiBoxAppend(h_box2, uiControl(Repair::label_status_), 1);
	uiBoxAppend(h_box2, uiControl(b_repair), 0);
	uiBoxAppend(v_box, uiControl(h_box2), 0);

	Repair::progressbar_ = uiNewProgressBar();
	uiBoxAppend(v_box, uiControl(Repair::progressbar_), 0);

	return uiControl(v_box);
}

#define CATCH_THEM(R) \
	catch (const char* e) {cerr << e << '\n'; msgBoxError(e, true); R;} \
	catch (string e) {cerr << e << '\n'; msgBoxError(e, true); R;} \
	catch (const std::exception& e) {cerr << e.what() << '\n'; msgBoxError(e.what(), true); R;}

uiControl* Gui::settingsTab() {
	auto v_box = newVerticalBox();

	auto group_general = newGroup("general");
	auto h_box_log = newHorizontalBox();
	auto label_log = uiNewLabel("log level");
	auto combo_log = uiNewCombobox();
	uiComboboxAppend(combo_log, "quiet (-q)");
	uiComboboxAppend(combo_log, "info (default)");
	uiComboboxAppend(combo_log, "hidden warnings (-w)");
	uiComboboxAppend(combo_log, "verbose (-v)");
	uiComboboxAppend(combo_log, "more verbose (-vv)");
	uiBoxAppend(h_box_log, uiControl(label_log), 0);
	uiBoxAppend(h_box_log, uiControl(combo_log), 0);
	uiGroupSetChild(group_general, uiControl(h_box_log));
	uiBoxAppend(v_box, uiControl(group_general), 0);

	auto group_repair = newGroup("repair");
	auto v_box_repair = newVerticalBox();
	auto chk_skip = uiNewCheckbox("skip unknown (-s)");

	auto h_box_step_size = newHorizontalBox();
	auto label_step_size = uiNewLabel("       step_size (-st)");
	auto num_step_size = uiNewSpinbox(1, 1<<16);
	uiBoxAppend(h_box_step_size, uiControl(label_step_size), 0);
	uiBoxAppend(h_box_step_size, uiControl(num_step_size), 0);

	auto chk_stretch = uiNewCheckbox("stretch video to match audio (-sv)");
	auto chk_keep_unknown = uiNewCheckbox("keep unknown sequences (-k)");
	auto chk_use_dyn = uiNewCheckbox("use dynamic stats (-dyn)");

	auto h_box_max_partsize = newHorizontalBox();
	auto label_max_partsize = uiNewLabel("      max partsize (-mp)");
	auto num_max_partsize = uiNewEntry();
	uiBoxAppend(h_box_max_partsize, uiControl(label_max_partsize), 0);
	uiBoxAppend(h_box_max_partsize, uiControl(num_max_partsize), 0);

	uiBoxAppend(v_box_repair, uiControl(chk_skip), 0);
	uiBoxAppend(v_box_repair, uiControl(h_box_step_size), 0);
	uiBoxAppend(v_box_repair, uiControl(chk_stretch), 0);
	uiBoxAppend(v_box_repair, uiControl(chk_keep_unknown), 0);
	uiBoxAppend(v_box_repair, uiControl(chk_use_dyn), 0);
	uiBoxAppend(v_box_repair, uiControl(h_box_max_partsize), 0);
	uiGroupSetChild(group_repair, uiControl(v_box_repair));
	uiBoxAppend(v_box, uiControl(group_repair), 0);

	uiComboboxSetSelected(combo_log, 1);
	uiComboboxOnSelected(combo_log, [](uiCombobox* c_box, void* data){
		auto v = uiComboboxSelected(c_box);
		if (v) v++;  // skip LogMode::W
		g_log_mode = (LogMode)v;

	}, nullptr);

	uiCheckboxSetChecked(chk_skip, g_ignore_unknown);
	uiCheckboxOnToggled(chk_skip, [](uiCheckbox* chk_box, void* spinbox){
		auto v = uiCheckboxChecked(chk_box);
		if (v) uiControlEnable(uiControl(spinbox));
		else uiControlDisable(uiControl(spinbox));
		g_ignore_unknown = v;

	}, num_step_size);

	if (!g_ignore_unknown) uiControlDisable(uiControl(num_step_size));
	uiSpinboxSetValue(num_step_size, Mp4::step_);
	uiSpinboxOnChanged(num_step_size, [](uiSpinbox* spinbox, void* data){
		Mp4::step_ = uiSpinboxValue(spinbox);
	}, nullptr);

	uiCheckboxSetChecked(chk_stretch, g_stretch_video);
	uiCheckboxOnToggled(chk_stretch, [](uiCheckbox* chk_box, void* data){
		auto v = uiCheckboxChecked(chk_box);
		if (v) msgBox("This feature is in beta, so it might not work!");
		g_stretch_video = v;

	}, nullptr);

	uiCheckboxSetChecked(chk_keep_unknown, g_dont_exclude);
	uiCheckboxOnToggled(chk_keep_unknown, [](uiCheckbox* chk_box, void* data){
		auto v = uiCheckboxChecked(chk_box);
		g_dont_exclude = v;

	}, nullptr);

	uiCheckboxSetChecked(chk_use_dyn, g_use_chunk_stats);
	uiCheckboxOnToggled(chk_use_dyn, [](uiCheckbox* chk_box, void* data){
		auto v = uiCheckboxChecked(chk_box);
		g_use_chunk_stats = v;

	}, nullptr);

	string s_max_partsize = to_string(g_max_partsize);
	uiEntrySetText(num_max_partsize, s_max_partsize.c_str());
	uiEntryOnChanged(num_max_partsize, [](uiEntry *entry, void *data){
		try {
			string s = uiEntryText(entry);
			if (s.empty()) s = "0";
			parseMaxPartsize(s);
		}
		CATCH_THEM(uiEntrySetText(entry, "0"));
	}, nullptr);

	return uiControl(v_box);
}

#define defineFileForAnalyze() \
	current_text_entry_ = Analyze::text_entry_; \
	uiMultilineEntrySetText(current_text_entry_, ""); \
	g_onProgress = Gui::Analyze::onProgress; \
	string file_ok = uiEntryText(Analyze::entry_ok_); \
	if (file_ok.empty()) { \
		msgBoxError("Please specify reference file!"); \
		return; \
	} \

#define ASYNC(fn, data) uiQueueMain((void (*)(void*))fn, (void*)data)

#define defineMp4ForAnalyze() \
	defineFileForAnalyze(); \
	Mp4 mp4; \
	try { \
		mp4.parseOk(file_ok); \
		g_mp4 = &mp4; \
	} \
	CATCH_THEM(return);

uiControl* Gui::analyzeTab() {
	auto v_box = newVerticalBox();

	auto h_box1 = newHorizontalBox();
	Analyze::entry_ok_ = addFileOpen(h_box1, "reference file");
	uiBoxAppend(v_box, uiControl(h_box1), 0);

	Analyze::text_entry_ = uiNewMultilineEntry();
	uiMultilineEntrySetReadOnly(Analyze::text_entry_, 1);
	uiBoxAppend(v_box, uiControl(Analyze::text_entry_), 1);

	auto h_box2 = newHorizontalBox();
	auto label = uiNewLabel("");
	auto b_dump = newButton("dump (-d)");
	auto b_analyze = newButton("analyze (-a)");
	auto b_atoms = newButton("atom search (-f)");
	auto b_info = newButton("info (-i)");
	uiBoxAppend(h_box2, uiControl(label), 1);
	uiBoxAppend(h_box2, uiControl(b_dump), 0);
	uiBoxAppend(h_box2, uiControl(b_analyze), 0);
	uiBoxAppend(h_box2, uiControl(b_atoms), 0);
	uiBoxAppend(h_box2, uiControl(b_info), 0);
	uiBoxAppend(v_box, uiControl(h_box2), 0);

	uiButtonOnClicked(b_dump, [](uiButton* b, void* data){
		defineMp4ForAnalyze();
		mp4.dumpSamples();
	}, NULL);

	uiButtonOnClicked(b_analyze, [](uiButton* b, void* data){
		defineMp4ForAnalyze();
		mp4.analyze();
	}, NULL);

	uiButtonOnClicked(b_info, [](uiButton* b, void* data){
		defineMp4ForAnalyze();
		mp4.printMediaInfo();
		cout << "\n\n";
		mp4.printAtoms();
	}, NULL);

	uiButtonOnClicked(b_atoms, [](uiButton* b, void* data){
		defineFileForAnalyze();  // for findAtomNames, file_ok can be truncated as well
		setDisabled(true);
		Analyze::onProgress(0);
		thread_ = new thread([](const string& file){
			try {
				Atom::findAtomNames(file);
				Analyze::onProgress(100);
			}
			CATCH_THEM();

			ASYNC(setDisabled, false);
		}, file_ok);
	}, NULL);

	Analyze::progressbar_ = uiNewProgressBar();
	uiBoxAppend(v_box, uiControl(Analyze::progressbar_), 0);

	return uiControl(v_box);
}

uiControl* Gui::aboutTab() {
	// show version + gh link
	auto v_box = newVerticalBox();

	auto label_ver = uiNewLabel(g_version_str.c_str());
	auto text_entry = uiNewMultilineEntry();
	uiMultilineEntrySetReadOnly(text_entry, 1);
	uiBoxAppend(v_box, uiControl(text_entry), 1);

	uiMultilineEntryAppend(text_entry, g_version_str.c_str());
	uiMultilineEntryAppend(text_entry,
						   "\n\n"
						   "Report your issues here:\n"
						   "https://github.com/anthwlock/untrunc"
						   );

	return uiControl(v_box);
}

void Gui::msgBox(const char* text) {
	uiMsgBox(window_, "Info", text);
}

void Gui::msgBoxError(const string& text, bool async) {
	if (async)
		ASYNC(msgBoxErrorQueued, new string(text));
	else
		msgBoxErrorSync(text);
}

void Gui::msgBoxErrorSync(const string& text) {
	auto txt = uiNewAttributedString(text.c_str());
	uiMsgBox(window_, "Error", uiAttributedStringString(txt));
	uiFreeAttributedString(txt);
//	uiMsgBox(window_, "Error", text);
}

void Gui::msgBoxErrorQueued(const string* s) {
	msgBoxErrorSync(*s);
	delete s;
}

void Gui::printToEntry(const char* text) {
	writeOutput(nullptr);
	uiMultilineEntryAppend(current_text_entry_, text);
}

void Gui::startRepair(uiButton* b, void* data) {
	current_text_entry_ = Repair::text_entry_;
	uiMultilineEntrySetText(current_text_entry_, "");

	string file_ok = uiEntryText(Repair::entry_ok_);
	string file_bad = uiEntryText(Repair::entry_bad_);
	if (file_ok.empty() || file_bad.empty()) {
		msgBoxError("Please specify the reference and the truncated file!");
		return;
	}

	setDisabled(true);
	Repair::onProgress(0);
	thread_ = new thread([](const string& file_ok, const string& file_bad){
		string output_suffix = g_ignore_unknown ? ss("-s", Mp4::step_) : "";

		try {
			Mp4 mp4;
			g_mp4 = &mp4;  // singleton is overkill, this is good enough
			g_onProgress = Gui::Repair::onProgress;
			mp4.parseOk(file_ok);

			mp4.repair(file_bad);
			chkHiddenWarnings();
			Repair::onProgress(100);
			cout << "\ndone!";

			if (mp4.premature_end_ && mp4.premature_percentage_ < 0.9) {
				ASYNC(msgBox, "Encountered premature end, please try '-s' in the \"Settings\" Tab.");
			}
		}
		CATCH_THEM();
		ASYNC(setDisabled, false);
	}, file_ok, file_bad);
}

void Gui::Repair::onProgress(int percentage) {
	uiProgressBarSetValue(Repair::progressbar_, percentage);
}

void Gui::Repair::onStatus(const string& status) {
	uiLabelSetText(Repair::label_status_, status.c_str());
}

void Gui::Analyze::onProgress(int percentage) {
	uiProgressBarSetValue(Analyze::progressbar_, percentage);
}
