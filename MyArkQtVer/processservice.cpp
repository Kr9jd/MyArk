#include "processservice.h"
#include <windows.h>
#include <tlhelp32.h>

std::vector<ProcessInfo> EnumProcesses()
{
    std::vector<ProcessInfo> processes;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return processes;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe))
    {
        do
        {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.parentPid = pe.th32ParentProcessID;
            info.threadCount = pe.cntThreads;
            info.name = QString::fromWCharArray(pe.szExeFile);

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc)
            {
                WCHAR pathBuf[MAX_PATH] = {};
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathLen))
                    info.path = QString::fromWCharArray(pathBuf);
                CloseHandle(hProc);
            }

            processes.push_back(std::move(info));
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return processes;
}

static QString ServiceTypeStr(DWORD type)
{
    if (type & SERVICE_KERNEL_DRIVER) return QStringLiteral("Kernel Driver");
    if (type & SERVICE_FILE_SYSTEM_DRIVER) return QStringLiteral("FS Driver");
    if (type & SERVICE_WIN32_OWN_PROCESS) return QStringLiteral("Own Process");
    if (type & SERVICE_WIN32_SHARE_PROCESS) return QStringLiteral("Shared Process");
    return QStringLiteral("Unknown");
}

static QString ServiceStateStr(DWORD state)
{
    switch (state)
    {
    case SERVICE_STOPPED:          return QStringLiteral("Stopped");
    case SERVICE_START_PENDING:    return QStringLiteral("Start Pending");
    case SERVICE_STOP_PENDING:     return QStringLiteral("Stop Pending");
    case SERVICE_RUNNING:          return QStringLiteral("Running");
    case SERVICE_CONTINUE_PENDING: return QStringLiteral("Continue Pending");
    case SERVICE_PAUSE_PENDING:    return QStringLiteral("Pause Pending");
    case SERVICE_PAUSED:           return QStringLiteral("Paused");
    default:                       return QStringLiteral("Unknown");
    }
}

static QString StartTypeStr(DWORD startType)
{
    switch (startType)
    {
    case SERVICE_AUTO_START:   return QStringLiteral("Auto");
    case SERVICE_BOOT_START:   return QStringLiteral("Boot");
    case SERVICE_DEMAND_START: return QStringLiteral("Manual");
    case SERVICE_DISABLED:     return QStringLiteral("Disabled");
    case SERVICE_SYSTEM_START: return QStringLiteral("System");
    default:                   return QStringLiteral("Unknown");
    }
}

std::vector<ServiceInfo> EnumServices()
{
    std::vector<ServiceInfo> services;

    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scManager)
        return services;

    DWORD bytesNeeded = 0;
    DWORD numberOfService = 0;
    EnumServicesStatusExW(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        nullptr, 0, &bytesNeeded, &numberOfService, nullptr, nullptr);

    DWORD bytesSize = bytesNeeded + sizeof(ENUM_SERVICE_STATUS_PROCESS);
    std::vector<unsigned char> buffer(bytesSize);

    if (EnumServicesStatusExW(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        buffer.data(), bytesSize, &bytesNeeded, &numberOfService, nullptr, nullptr))
    {
        auto* servicesArray = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(buffer.data());
        for (DWORD i = 0; i < numberOfService; ++i)
        {
            ServiceInfo info;
            info.name        = QString::fromWCharArray(servicesArray[i].lpServiceName);
            info.displayName = QString::fromWCharArray(servicesArray[i].lpDisplayName);
            info.serviceType = servicesArray[i].ServiceStatusProcess.dwServiceType;
            info.currentState = servicesArray[i].ServiceStatusProcess.dwCurrentState;
            info.startType = servicesArray[i].ServiceStatusProcess.dwControlsAccepted;
            services.push_back(std::move(info));
        }
    }

    CloseServiceHandle(scManager);
    return services;
}
