#pragma once
#include <windows.h>
#include <vector>
#include <QString>

struct ProcessInfo {
    DWORD pid;
    DWORD parentPid;
    QString name;
    QString path;
    size_t threadCount;
};

struct ServiceInfo {
    QString name;
    QString displayName;
    DWORD serviceType;
    DWORD currentState;
    DWORD startType;
};

std::vector<ProcessInfo> EnumProcesses();
std::vector<ServiceInfo> EnumServices();
