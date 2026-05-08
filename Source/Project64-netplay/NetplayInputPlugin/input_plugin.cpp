#include "stdafx.h"

#include "input_plugin.h"
#include "id_variable.h"
#include "util.h"

using namespace std;

input_plugin::input_plugin(string path) {
    dll = LoadLibrary(utf8_to_wstring(path).c_str());

    if (!dll) {
        DWORD error_code = GetLastError();
        throw runtime_error("Could not load base input plugin dll. Error Code: " + to_string(error_code));
    }

    if (GetProcAddress(dll, _IDENTIFYING_VARIABLE_NAME)) {
        FreeLibrary(dll);
        throw runtime_error("Cannot load another Netplay plugin");
    }

    GetDllInfo = (void(*)(PLUGIN_INFO * PluginInfo)) GetProcAddress(dll, "GetDllInfo");

    info.Type = (WORD)(-1);
    if (GetDllInfo) {
        GetDllInfo(&info);
    }

    if (info.Type != PLUGIN_TYPE_CONTROLLER) {
        FreeLibrary(dll);
        throw runtime_error("Plugin is not an input plugin");
    }

    if (info.Version != 0x0100 && info.Version != 0x0101) {
        FreeLibrary(dll);
        throw runtime_error("Plugin must be version 1.0 or 1.1");
    }

    if (info.Version == 0x0100) {
        InitiateControllers0100 = (void(*)(HWND hMainWindow, CONTROL Controls[4])) GetProcAddress(dll, "InitiateControllers");
    } else if (info.Version == 0x0101) {
        InitiateControllers0101 = (void(*)(CONTROL_INFO Controls)) GetProcAddress(dll, "InitiateControllers");
    }
    CloseDLL                 = (void(*)(void))                                   GetProcAddress(dll, "CloseDLL");
    ControllerCommand        = (void(*)(int, BYTE *))                            GetProcAddress(dll, "ControllerCommand");
    DllAbout                 = (void(*)(HWND hParent))                           GetProcAddress(dll, "DllAbout");
    DllConfig                = (void(*)(HWND hParent))                           GetProcAddress(dll, "DllConfig");
    DllTest                  = (void(*)(HWND hParent))                           GetProcAddress(dll, "DllTest");
    GetKeys                  = (void(*)(int Control, BUTTONS * Keys))            GetProcAddress(dll, "GetKeys");
    ReadController           = (void(*)(int Control, BYTE * Command))            GetProcAddress(dll, "ReadController");
    RomClosed                = (void(*)(void))                                   GetProcAddress(dll, "RomClosed");
    RomOpen                  = (void(*)(void))                                   GetProcAddress(dll, "RomOpen");
    WM_KeyDown               = (void(*)(WPARAM wParam, LPARAM lParam))           GetProcAddress(dll, "WM_KeyDown");
    WM_KeyUp                 = (void(*)(WPARAM wParam, LPARAM lParam))           GetProcAddress(dll, "WM_KeyUp");
}

input_plugin::~input_plugin() {
    CloseDLL();
    FreeLibrary(dll);
}

bool input_plugin::initiate_controllers(CONTROL_INFO info) {
    for (int i = 0; i < 4; i++) {
        controls[i].Present = false;
        controls[i].RawData = false;
        controls[i].Plugin = PLUGIN_NONE;
    }

    if (InitiateControllers0100) {
        InitiateControllers0100(info.hMainWindow, controls);
        controllers_initiated = true;
    } else if (InitiateControllers0101) {
        info.Controls = controls;
        InitiateControllers0101(info);
        controllers_initiated = true;
    }

    return controllers_initiated;
}