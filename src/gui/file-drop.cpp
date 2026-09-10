#include "file-drop.h"

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>
#include <ole2.h>
#include <objidl.h>
#include <oleidl.h>

#include <map>
#include <vector>

#include "../common.h"
using namespace std;

namespace {

map<HWND, FileDrop::Handler> pickers;

FileDrop::Handler* pickerAt(const POINT& pt) {
	for (HWND w = WindowFromPoint(pt); w; w = GetParent(w)) {
		auto it = pickers.find(w);
		if (it != pickers.end())
			return &it->second;
	}
	return nullptr;
}

POINT cursorPos() {
	POINT pt;
	GetCursorPos(&pt);
	return pt;
}

DWORD effectForCursor() {
	return pickerAt(cursorPos()) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

bool acceptsFiles(IDataObject* data) {
	if (!data) return false;
	FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	return data->QueryGetData(&fmt) == S_OK;
}

vector<string> getFiles(IDataObject* data) {
	vector<string> files;
	FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM stg = {};
	if (FAILED(data->GetData(&fmt, &stg))) return files;

	HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
	if (hDrop) {
		UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
		for (UINT i = 0; i < n; i++) {
			UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
			if (!len) continue;
			wstring ws(len, L'\0');
			DragQueryFileW(hDrop, i, &ws[0], (UINT)ws.size() + 1);
			files.emplace_back(to_utf8(ws.c_str()));
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	return files;
}

class FileDropTarget : public IDropTarget {
public:
	FileDropTarget() : refs_(1) {}

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
		static const GUID iid_unk = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
		static const GUID iid_dt  = {0x00000122, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
		if (riid == iid_unk || riid == iid_dt) {
			*ppv = this;
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
	STDMETHODIMP_(ULONG) Release() override {
		ULONG n = --refs_;
		if (n == 0) delete this;
		return n;
	}

	STDMETHODIMP DragEnter(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
		*effect = acceptsFiles(data) ? effectForCursor() : DROPEFFECT_NONE;
		return S_OK;
	}

	STDMETHODIMP DragOver(DWORD, POINTL, DWORD* effect) override {
		*effect = effectForCursor();
		return S_OK;
	}

	STDMETHODIMP DragLeave() override { return S_OK; }

	STDMETHODIMP Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
		*effect = effectForCursor();
		if (*effect == DROPEFFECT_NONE || !data) return S_OK;

		FileDrop::Handler* handler = pickerAt(cursorPos());
		if (!handler) return S_OK;

		vector<string> files = getFiles(data);
		if (files.size() == 1)  // only single-file drops are accepted
			(*handler)(files[0]);
		return S_OK;
	}

private:
	ULONG refs_;
};

FileDropTarget* s_target = nullptr;  // kept alive for the app's lifetime

}  // namespace

void FileDrop::enable(uiWindow* window) {
	if (!window) return;
	HWND hwnd = (HWND)uiControlHandle(uiControl(window));
	if (!hwnd) return;

	if (SUCCEEDED(OleInitialize(nullptr))) {
		s_target = new FileDropTarget();
		if (FAILED(RegisterDragDrop(hwnd, s_target))) {
			delete s_target;
			s_target = nullptr;
		}
	}
}

void FileDrop::registerPicker(uiControl* control, uiEntry* target) {
	if (!control || !target) return;
	HWND hwnd = (HWND)uiControlHandle(control);
	if (!hwnd) return;

	pickers[hwnd] = [target](const string& f) { uiEntrySetText(target, f.c_str()); };
}

#endif  // _WIN32