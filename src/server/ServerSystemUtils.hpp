#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstdio>

namespace Mercenary {

class ServerSystemUtils {
public:
    static bool SetupFirewallRule(uint16_t port, std::string& outMsg) {
        char cmd[512];
        std::snprintf(cmd, sizeof(cmd),
            "netsh advfirewall firewall add rule name=\"Mercenary Dedicated Server (UDP %u)\" dir=in action=allow protocol=UDP localport=%u",
            port, port);

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (exitCode == 0) {
                outMsg = "SUCCESS: Windows Defender Firewall rule added for UDP port " + std::to_string(port);
                return true;
            } else {
                outMsg = "Warning: netsh returned exit code " + std::to_string(exitCode) + " (Admin rights may be required)";
                return false;
            }
        }
        outMsg = "Failed to spawn netsh process.";
        return false;
    }

    static bool CheckFirewallRule(uint16_t port) {
        char cmd[512];
        std::snprintf(cmd, sizeof(cmd),
            "netsh advfirewall firewall show rule name=\"Mercenary Dedicated Server (UDP %u)\"", port);

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3000);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return (exitCode == 0);
        }
        return false;
    }

    static const char* SERVICE_NAME() { return "MercenaryDedicatedServer"; }
    static const char* SERVICE_DISPLAY_NAME() { return "Mercenary Continental Dedicated Server"; }

    static int GetServiceState() {
        SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
        if (!hSCM) return 0; // Not accessible or not installed

        SC_HANDLE hService = OpenServiceA(hSCM, SERVICE_NAME(), SERVICE_QUERY_STATUS);
        if (!hService) {
            CloseServiceHandle(hSCM);
            return 0; // Not installed
        }

        SERVICE_STATUS_PROCESS ssp{};
        DWORD bytesNeeded = 0;
        int state = 1; // Installed & Stopped
        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            if (ssp.dwCurrentState == SERVICE_RUNNING) {
                state = 2; // Running
            } else if (ssp.dwCurrentState == SERVICE_START_PENDING || ssp.dwCurrentState == SERVICE_STOP_PENDING) {
                state = 3; // Pending
            }
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return state;
    }

    static bool InstallService(uint16_t port, std::string& outMsg) {
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
            outMsg = "Failed to determine server executable path.";
            return false;
        }

        std::string binaryPath = std::string("\"") + exePath + "\" --service --port " + std::to_string(port);

        SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!hSCM) {
            outMsg = "Cannot access Service Control Manager (Run as Administrator required).";
            return false;
        }

        SC_HANDLE hService = CreateServiceA(
            hSCM,
            SERVICE_NAME(),
            SERVICE_DISPLAY_NAME(),
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            binaryPath.c_str(),
            NULL, NULL, NULL, NULL, NULL
        );

        if (!hService) {
            DWORD err = GetLastError();
            CloseServiceHandle(hSCM);
            if (err == ERROR_SERVICE_EXISTS) {
                outMsg = "Service already installed.";
                return true;
            }
            outMsg = "CreateService failed (Error code: " + std::to_string(err) + ")";
            return false;
        }

        // Add service description
        SERVICE_DESCRIPTIONA sd;
        char desc[] = "Mercenary Warzone 24/7 Authoritative Continental Game Server with sub-tick ballistics and 6-hex world.";
        sd.lpDescription = desc;
        ChangeServiceConfig2A(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        outMsg = "SUCCESS: Windows Service installed (AUTO-START on boot)!";
        return true;
    }

    static bool UninstallService(std::string& outMsg) {
        SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!hSCM) {
            outMsg = "Cannot access Service Control Manager (Run as Administrator required).";
            return false;
        }

        SC_HANDLE hService = OpenServiceA(hSCM, SERVICE_NAME(), SERVICE_STOP | DELETE);
        if (!hService) {
            CloseServiceHandle(hSCM);
            outMsg = "Service is not installed.";
            return true;
        }

        // Try to stop first
        SERVICE_STATUS ss;
        ControlService(hService, SERVICE_CONTROL_STOP, &ss);

        if (!DeleteService(hService)) {
            DWORD err = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            outMsg = "DeleteService failed (Error: " + std::to_string(err) + ")";
            return false;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        outMsg = "SUCCESS: Windows Service removed.";
        return true;
    }

    static bool StartWindowsService(std::string& outMsg) {
        SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!hSCM) {
            outMsg = "Cannot access Service Control Manager.";
            return false;
        }

        SC_HANDLE hService = OpenServiceA(hSCM, SERVICE_NAME(), SERVICE_START);
        if (!hService) {
            CloseServiceHandle(hSCM);
            outMsg = "Service not found. Install it first.";
            return false;
        }

        if (!StartServiceA(hService, 0, NULL)) {
            DWORD err = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            outMsg = "StartService failed (Error: " + std::to_string(err) + ")";
            return false;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        outMsg = "SUCCESS: Service started in background.";
        return true;
    }

    static bool StopWindowsService(std::string& outMsg) {
        SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!hSCM) {
            outMsg = "Cannot access Service Control Manager.";
            return false;
        }

        SC_HANDLE hService = OpenServiceA(hSCM, SERVICE_NAME(), SERVICE_STOP);
        if (!hService) {
            CloseServiceHandle(hSCM);
            outMsg = "Service not found.";
            return false;
        }

        SERVICE_STATUS ss;
        if (!ControlService(hService, SERVICE_CONTROL_STOP, &ss)) {
            DWORD err = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            outMsg = "StopService failed (Error: " + std::to_string(err) + ")";
            return false;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        outMsg = "SUCCESS: Service stopped.";
        return true;
    }
};

} // namespace Mercenary
