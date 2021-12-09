#pragma once

#include <QString>
#include <cstdint>
#include <cstddef>
#include <tuple>

struct UniqueProcess
{
    uint32_t pid = -1;
    uint32_t ppid = -1;
    QString name;

    bool operator<(const UniqueProcess& o) const
    {
        return std::tie(pid, ppid, name) < std::tie(o.pid, o.ppid, o.name);
    }
};

struct ProcessData
{
    uint64_t time = 0;
    uint64_t memoryUsage = 0;
    uint64_t pagefileUsage = 0;
    double cpuUsage = 0.0;
};

struct SortedProcess
{
    UniqueProcess uniqueProcess;
    uint64_t startTime = -1;
    uint64_t endTime = 0;
    size_t maxMemoryUsage = 0;

    bool operator<(const SortedProcess& o) const
    {
        return std::tie(startTime, endTime, uniqueProcess.pid, uniqueProcess.ppid) < std::tie(o.startTime, o.endTime, o.uniqueProcess.pid, o.uniqueProcess.ppid);
    }
};

static void humanReadableSize(size_t sizeInBytes, char* buf, size_t cb)
{
    static const char* sizeUnits[] = { "B", "KB", "MB", "GB", "TB", "PB" };

    size_t sizeType = 0;
    double actualSize = (double)sizeInBytes;

    while (actualSize > 1024)
    {
        actualSize /= 1024;
        sizeType++;
    }

    sprintf_s(buf, cb, "%.03f %s", actualSize, sizeUnits[sizeType]);
}

static QString humanReadableSize(size_t sizeInBytes)
{
    char temp[128] = "";
    humanReadableSize(sizeInBytes, temp, _countof(temp));
    return temp;
}
