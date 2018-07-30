/*
 *  Final h-encore, Copyright (C) 2018  Soar Qin
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "finalhe.hh"

#include <QtWidgets/QApplication>
#include <QSettings>
#include <QMessageBox>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#endif

#if defined(QT_STATIC)
#include <QtPlugin>
#if defined(_WIN32)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
#if defined(__APPLE__)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif

#ifdef _WIN32

struct WndInfo {
    DWORD procId;
    HWND hwnd;
};

bool findQCMA(WndInfo *wndInfo) {
    wndInfo->procId = 0;
    EnumWindows([](HWND hwnd, LPARAM lParam)->BOOL {
        CHAR n[128];
        if (GetClassNameA(hwnd, n, 128) && lstrcmpA(n, "QTrayIconMessageWindowClass") == 0) {
            DWORD procId;
            GetWindowThreadProcessId(hwnd, &procId);
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, procId);
            if (hProc == INVALID_HANDLE_VALUE) return TRUE;
            CHAR fname[256];
            if (GetModuleFileNameExA(hProc, NULL, fname, 256) <= 0) {
                CloseHandle(hProc);
                return TRUE;
            }
            if (lstrcmpiA(PathFindFileNameA(fname), "qcma.exe") == 0) {
                WndInfo *info = (WndInfo*)lParam;
                info->procId = procId;
                info->hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)wndInfo);
    return wndInfo->procId != 0;
}

#endif

int main(int argc, char *argv[]) {
#if defined(_DEBUG) && defined(_WIN32)
    if (::AllocConsole()) {
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("soarqin");
    QCoreApplication::setOrganizationDomain("soar.im");
    QCoreApplication::setApplicationName("FinalHE");
    QSettings::setDefaultFormat(QSettings::IniFormat);

#ifdef _WIN32
    WndInfo wndInfo;
    if (findQCMA(&wndInfo)) {
        if (QMessageBox::question(nullptr, FinalHE::tr("WARNING"), FinalHE::tr("Qcma is running, force close it now?")) == QMessageBox::Yes) {
            if (PostMessageA(wndInfo.hwnd, WM_QUIT, 0, 0), Sleep(1000), findQCMA(&wndInfo)) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, wndInfo.procId);
                if (!TerminateProcess(hProc, 0) || (Sleep(1000), findQCMA(&wndInfo))) {
                    QMessageBox::critical(nullptr, FinalHE::tr("ERROR"), FinalHE::tr("Unable to close Qcma, please close it manually and then restart this tool."));
                }
            }
        } else {
            a.quit();
            return 0;
        }
    }
#endif

    FinalHE w;
    w.show();
    return a.exec();
}
