#define NOMINMAX
#include "native.h"
#include <Psapi.h>

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <ctime>

#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <iterator>

static wchar_t szCommandLine[2048];
static std::atomic<bool> bStopMonitoringThread;

//Conversion functions taken from: http://www.nubaria.com/en/blog/?p=289
static std::string Utf16ToUtf8(const wchar_t* wstr, size_t len = -1)
{
	std::string convertedString;
	if (!wstr || !*wstr)
		return convertedString;
	if (len == -1)
		len = wcslen(wstr);
	auto requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, nullptr, 0, nullptr, nullptr);
	if (requiredSize > 0)
	{
		convertedString.resize(requiredSize);
		if (!WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, (char*)convertedString.c_str(), requiredSize, nullptr, nullptr))
			convertedString.clear();
	}
	return convertedString;
}

static std::string Utf16ToUtf8(const std::wstring& wstr)
{
	return Utf16ToUtf8(wstr.c_str());
}

static std::string Utf16ToUtf8(const UNICODE_STRING& wstr)
{
	return Utf16ToUtf8(wstr.Buffer, wstr.Length / sizeof(*wstr.Buffer));
}

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

	if (sizeType < _countof(sizeUnits))
		sprintf_s(buf, cb, "%.03f %s", actualSize, sizeUnits[sizeType]);
}

static std::string humanReadableSize(size_t sizeInBytes)
{
	char temp[128] = "";
	humanReadableSize(sizeInBytes, temp, _countof(temp));
	return temp;
}

struct LastCpuUsage
{
	ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
};

#define HandleToPid(h) DWORD(ULONG_PTR(h))

struct UniqueProcess
{
	DWORD pid = -1;
	DWORD ppid = -1;
	std::string name;

	UniqueProcess() = default;
	UniqueProcess(const PSYSTEM_PROCESS_INFORMATION process) :
		pid(HandleToPid(process->UniqueProcessId)),
		ppid(HandleToPid(process->InheritedFromUniqueProcessId)),
		name(Utf16ToUtf8(process->ImageName))
	{
	}

	static auto tie(const UniqueProcess& p)
	{
		return std::tie(p.pid, p.ppid, p.name);
	}

	static auto tie(const UniqueProcess* p)
	{
		return tie(*p);
	}

	bool operator<(const UniqueProcess& o) const
	{
		return tie(this) < tie(o);
	}

	bool operator==(const UniqueProcess& o) const
	{
		return tie(this) == tie(o);
	}

	bool operator!=(const UniqueProcess& o) const
	{
		return !(*this == o);
	}
};

template <typename Cont, typename Pred>
Cont filter(const Cont& container, Pred predicate)
{
	Cont result;
	std::copy_if(container.begin(), container.end(), std::back_inserter(result), predicate);
	return result;
}

static uint64_t convertTime(const SYSTEMTIME& st)
{
	tm tm;
	tm.tm_sec = st.wSecond;
	tm.tm_min = st.wMinute;
	tm.tm_hour = st.wHour;
	tm.tm_mday = st.wDay;
	tm.tm_mon = st.wMonth - 1;
	tm.tm_year = st.wYear - 1900;
	tm.tm_isdst = -1;
	return mktime(&tm) * 1000 + st.wMilliseconds;
}

class ProcessTimeSeries
{
	struct ProcessData
	{
		uint64_t time = 0;
		PROCESS_MEMORY_COUNTERS_EX memory = { 0 };
		double cpuUsage = 0.0;

		ProcessData() = default;

		ProcessData(uint64_t time, const PROCESS_MEMORY_COUNTERS_EX& memory, double cpuUsage) :
			time(time),
			memory(memory),
			cpuUsage(cpuUsage)
		{
		}

		void toJson(FILE* file) const
		{
			fprintf(file, R"({"time":%llu,"cpuUsage":%.0f,"memory":{"pageFaultCount":%u,"peakWorkingSetSize":%zu,"workingSetSize":%zu,"quotaPeakPagedPoolUsage":%zu,"quotaPagedPoolUsage":%zu,"quotaPeakNonPagedPoolUsage":%zu,"quotaNonPagedPoolUsage":%zu,"pagefileUsage":%zu,"peakPagefileUsage":%zu,"privateUsage":%zu}})",
				time,
				cpuUsage,
				memory.PageFaultCount,
				memory.PeakWorkingSetSize,
				memory.WorkingSetSize,
				memory.QuotaPeakPagedPoolUsage,
				memory.QuotaPagedPoolUsage,
				memory.QuotaPeakNonPagedPoolUsage,
				memory.QuotaNonPagedPoolUsage,
				memory.PagefileUsage,
				memory.PeakPagefileUsage,
				memory.PrivateUsage
			);
		}
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

	FILE* m_logFile = nullptr;
	std::map<UniqueProcess, LastCpuUsage> m_lastCpu;
	std::map<UniqueProcess, std::vector<ProcessData>> m_processData;

public:
	ProcessTimeSeries(FILE* logFile) : m_logFile(logFile) { }

	FILE* logFile() { return m_logFile; }

	void startTick(const SYSTEMTIME& time, DWORD monitoredPid)
	{
		fprintf(m_logFile, "[%02d:%02d:%02d.%03d] Tracked processes (monitored: %u):\n",
			time.wHour,
			time.wMinute,
			time.wSecond,
			time.wMilliseconds,
			monitoredPid
		);
		fflush(m_logFile);
	}

	void logTickData(const SYSTEMTIME& time, const PSYSTEM_PROCESS_INFORMATION process)
	{
		UniqueProcess uniqueProcess(process);
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, uniqueProcess.pid);
		if (hProcess)
		{
			PROCESS_MEMORY_COUNTERS_EX memoryCounters = { 0 };
			if (GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&memoryCounters, sizeof(PROCESS_MEMORY_COUNTERS_EX)))
			{
				auto cpuUsage = getCurrentCPUUsage(uniqueProcess, hProcess);
				fprintf(m_logFile, "  %s (PID: %u, Parent: %u)\n",
					uniqueProcess.name.c_str(),
					uniqueProcess.pid,
					uniqueProcess.ppid
				);
				fprintf(m_logFile, "    Memory usage: %s, Memory peak: %s, Pagefile usage: %s, Pagefile peak: %s ~ CPU: %.0f%%\n",
					humanReadableSize(memoryCounters.WorkingSetSize).c_str(),
					humanReadableSize(memoryCounters.PeakWorkingSetSize).c_str(),
					humanReadableSize(memoryCounters.PagefileUsage).c_str(),
					humanReadableSize(memoryCounters.PeakPagefileUsage).c_str(),
					cpuUsage
				);
				fflush(m_logFile);
				m_processData[uniqueProcess].emplace_back(
					convertTime(time),
					memoryCounters,
					cpuUsage
				);
			}
			CloseHandle(hProcess);
		}
	}

	bool dumpCsv(const std::string& file) const
	{
		FILE* csvFile = _fsopen(file.c_str(), "wb", _SH_DENYWR);
		if (!csvFile)
			return false;
		std::map<uint64_t, std::map<UniqueProcess, ProcessData>> timeline;
		fprintf(csvFile, "Time");
		auto sortedProcesses = getSortedProcesses();
		sortedProcesses = filter(sortedProcesses, [](const SortedProcess& p)
			{
				return p.maxMemoryUsage > 1024 * 1024 * 100;
			});
		for (const auto& process : sortedProcesses)
		{
			const UniqueProcess& uniqueProcess = process.uniqueProcess;
			for (const ProcessData& data : m_processData.at(uniqueProcess))
				timeline[data.time].emplace(uniqueProcess, data);
			fprintf(csvFile, ";%s (pid: %u, ppid: %u)", uniqueProcess.name.c_str(), uniqueProcess.pid, uniqueProcess.ppid);
		}
		fprintf(csvFile, "\r\n");
		for (const auto& event : timeline)
		{
			fprintf(csvFile, "%llu", event.first);
			for (const auto& process : sortedProcesses)
			{
				fprintf(csvFile, ";");
				auto itr = event.second.find(process.uniqueProcess);
				if (itr != event.second.end())
				{
					fprintf(csvFile, "%zu", itr->second.memory.WorkingSetSize / 1024 / 1024);
				}
			}
			fprintf(csvFile, "\r\n");
		}
		fclose(csvFile);
		return true;
	}

	bool dumpJson(const std::string& file) const
	{
		FILE* jsonFile = _fsopen(file.c_str(), "wb", _SH_DENYWR);
		if (!jsonFile)
			return false;
		fprintf(jsonFile, "[");
		bool firstProcess = true;
		auto sortedProcesses = getSortedProcesses();
		for (const auto& process : sortedProcesses)
		{
			fprintf(jsonFile, "%s{", firstProcess ? "" : ",");
			const UniqueProcess& uniqueProcess = process.uniqueProcess;
			fprintf(jsonFile, R"("pid":%u,"ppid":%u,"name":"%s","data":[)",
				uniqueProcess.pid,
				uniqueProcess.ppid,
				uniqueProcess.name.c_str()
			);
			bool firstData = true;
			for (const ProcessData& data : m_processData.at(uniqueProcess))
			{
				if (!firstData)
					fprintf(jsonFile, ",");
				data.toJson(jsonFile);
				firstData = false;
			}
			fprintf(jsonFile, "]}");
			firstProcess = false;
		}
		fprintf(jsonFile, "]");
		fclose(jsonFile);
		return true;
	}

private:
	double getCurrentCPUUsage(const UniqueProcess& p, HANDLE hProcess)
	{
		static int numProcessors = 0;

		if (numProcessors == 0)
		{
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			numProcessors = sysInfo.dwNumberOfProcessors;
		}

		auto itr = m_lastCpu.find(p);
		if (itr == m_lastCpu.end())
		{
			LastCpuUsage last;
			FILETIME ftime, fsys, fuser;
			GetSystemTimeAsFileTime(&ftime);
			memcpy(&last.lastCPU, &ftime, sizeof(FILETIME));

			GetProcessTimes(hProcess, &ftime, &ftime, &fsys, &fuser);
			memcpy(&last.lastSysCPU, &fsys, sizeof(FILETIME));
			memcpy(&last.lastUserCPU, &fuser, sizeof(FILETIME));
			itr = m_lastCpu.emplace(p, last).first;
		}

		LastCpuUsage& last = itr->second;

		FILETIME ftime, fsys, fuser;
		ULARGE_INTEGER now, sys, user;

		GetSystemTimeAsFileTime(&ftime);
		memcpy(&now, &ftime, sizeof(FILETIME));

		GetProcessTimes(hProcess, &ftime, &ftime, &fsys, &fuser);

		memcpy(&sys, &fsys, sizeof(FILETIME));
		memcpy(&user, &fuser, sizeof(FILETIME));
		auto percent = double((sys.QuadPart - last.lastSysCPU.QuadPart) + (user.QuadPart - last.lastUserCPU.QuadPart));
		percent /= (now.QuadPart - last.lastCPU.QuadPart);
		percent /= numProcessors;
		last.lastCPU = now;
		last.lastUserCPU = user;
		last.lastSysCPU = sys;

		// hack to not get corrupt data
		if (isnan(percent))
			percent = 0.0;

		return percent * 100.0;
	}

	std::vector<SortedProcess> getSortedProcesses() const
	{
		std::vector<SortedProcess> sortedProcesses;
		for (const auto& process : m_processData)
		{
			SortedProcess s;
			s.uniqueProcess = process.first;
			for (const ProcessData& data : process.second)
			{
				s.startTime = std::min(data.time, s.startTime);
				s.endTime = std::max(data.time, s.endTime);
				s.maxMemoryUsage = std::max((size_t)data.memory.WorkingSetSize, s.maxMemoryUsage);
			}
			sortedProcesses.push_back(s);
		}
		std::sort(sortedProcesses.begin(), sortedProcesses.end());
		return sortedProcesses;
	}
};

static void enumerateProcesses(DWORD monitoredPid, ProcessTimeSeries& timeSeries)
{
	static std::vector<UniqueProcess> processListCache;
	SYSTEMTIME lt;
	GetLocalTime(&lt);

	ULONG Length = 0;
again:
	auto status = NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &Length);
	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		auto data = new uint8_t[Length * 2];
		memset(data, 0, Length * 2);
		status = NtQuerySystemInformation(SystemProcessInformation, data, Length * 2, NULL);
		if (status == STATUS_SUCCESS)
		{
			std::map<DWORD, PSYSTEM_PROCESS_INFORMATION> processes; // pid -> info
			std::map<DWORD, std::vector<DWORD>> tree; // parentpid -> [childpids]
			// Build tree of all processes running on the system
			{
				std::vector<UniqueProcess> processList;
				PSYSTEM_PROCESS_INFORMATION process = PSYSTEM_PROCESS_INFORMATION(data);
#define NEXT_PROCESS(p) (p->NextEntryOffset ? PSYSTEM_PROCESS_INFORMATION((uint8_t*)p + p->NextEntryOffset) : nullptr)
				do
				{
					// https://chromium.googlesource.com/chromium/src/tools/win/+/053790b0f1a7aa314dc594758428a55c00e107d0/IdleWakeups/system_information_sampler.cpp#247
					if (ULONG_PTR(process) + sizeof(SYSTEM_PROCESS_INFORMATION) >= ULONG_PTR(data) + Length * 2)
						break;
					processList.emplace_back(process);
					auto pid = HandleToPid(process->UniqueProcessId);
					processes[pid] = process;
				} while (process = NEXT_PROCESS(process));
#undef NEXT_PROCESS

				for (const auto& process : processList)
					tree[process.ppid].push_back(process.pid);

				if (processList != processListCache)
				{
					fprintf(timeSeries.logFile(), "Updated process list:\n");
					processListCache = processList;
					for (const auto& process : processListCache)
						fprintf(timeSeries.logFile(), "  \"%s\" (PID: %u, Parent: %u)\n",
							process.name.c_str(),
							process.pid,
							process.ppid);
					fflush(timeSeries.logFile());
				}
			}

			timeSeries.startTick(lt, monitoredPid);
			// Breath first search, starting from the monitored pid
			if (processes.count(monitoredPid))
			{
				// BFS
				std::queue<DWORD> queue;
				std::set<DWORD> visited;
				visited.insert(GetCurrentProcessId());
				queue.push(monitoredPid);
				while (!queue.empty())
				{
					DWORD pid = queue.front();
					queue.pop();
					if (visited.count(pid))
						continue;
					visited.insert(pid);
					for (auto childPid : tree[pid])
						queue.push(childPid);
					if (processes.count(pid))
						timeSeries.logTickData(lt, processes.at(pid));
				}
			}
		}
		else
		{
			delete[] data;
			goto again;
		}
		delete[] data;
	}
	else
		goto again;
}

static DWORD WINAPI MonitoringThread(LPVOID param)
{
	auto pi = PPROCESS_INFORMATION(param);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	char basename[256] = "";
	sprintf_s(basename, "Onlooker_%04d-%02d-%02d_%02d-%02d-%02d_%u",
		lt.wYear,
		lt.wMonth,
		lt.wDay,
		lt.wHour,
		lt.wMinute,
		lt.wSecond,
		pi->dwProcessId
	);
	FILE* logFile = _fsopen((std::string(basename) + ".log").c_str(), "wb", _SH_DENYWR);
	if (!logFile)
	{
		fwprintf(stderr, L"[Onlooker] Failed to open log file.\n");
		return 0;
	}

	{
		SYSTEMTIME lt;
		GetLocalTime(&lt);
		auto time = convertTime(lt);

		fprintf(logFile, "Onlooker PID: 0x%X (%u)\n", GetCurrentProcessId(), GetCurrentProcessId());
		fprintf(logFile, "Time: %llu (%04d-%02d-%02d %02d:%02d:%02d.%d)\n",
			time,
			lt.wYear,
			lt.wMonth,
			lt.wDay,
			lt.wHour,
			lt.wMinute,
			lt.wSecond,
			lt.wMilliseconds
		);
		char szComputerName[MAX_PATH] = "";
		DWORD size = _countof(szComputerName);
		GetComputerNameA(szComputerName, &size);
		fprintf(logFile, "Computer name: %s\n", szComputerName);
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx(&statex);
		fprintf(logFile, "There is %u percent of memory in use.\n", statex.dwMemoryLoad);
		fprintf(logFile, "There are %s total of physical memory.\n", humanReadableSize(statex.ullTotalPhys).c_str());
		fprintf(logFile, "There are %s free of physical memory.\n", humanReadableSize(statex.ullAvailPhys).c_str());
		fprintf(logFile, "There are %s total of paging file.\n", humanReadableSize(statex.ullTotalPageFile).c_str());
		fprintf(logFile, "There are %s free of paging file.\n", humanReadableSize(statex.ullAvailPageFile).c_str());
		fprintf(logFile, "There are %s total of virtual memory.\n", humanReadableSize(statex.ullTotalVirtual).c_str());
		fprintf(logFile, "There are %s free of virtual memory.\n", humanReadableSize(statex.ullAvailVirtual).c_str());
		fprintf(logFile, "\n");
		fflush(logFile);
	}

	ProcessTimeSeries timeSeries(logFile);

	DWORD pollInterval = 100;
	char szPollInterval[32] = "";
	if (GetEnvironmentVariableA("ONLOOKER_POLL_INTERVAL", szPollInterval, std::size(szPollInterval)) && *szPollInterval)
	{
		if (sscanf(szPollInterval, "%u", &pollInterval) != 1)
			pollInterval = 100;
	}

	while (!bStopMonitoringThread)
	{
		DWORD ticks = GetTickCount();

		if (pi->dwProcessId)
			enumerateProcesses(pi->dwProcessId, timeSeries);

		DWORD elapsed = GetTickCount() - ticks;

		Sleep(std::min(pollInterval, pollInterval - elapsed));
	}

	if (!timeSeries.dumpJson(std::string(basename) + ".json"))
	{
		fwprintf(stderr, L"[Onlooker] Failed to open json file.\n");
		return 0;
	}

	if (!timeSeries.dumpCsv(std::string(basename) + ".csv"))
	{
		fwprintf(stderr, L"[Onlooker] Failed to open csv file.\n");
		return 0;
	}

	fclose(logFile);

	return 0;
}

static bool FileExists(const wchar_t* file)
{
	DWORD attrib = GetFileAttributesW(file);
	return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

int wmain(int argc, wchar_t* argv[])
{
	wchar_t szProxyModule[MAX_PATH] = L"";
	if (!GetModuleFileNameW(GetModuleHandleW(NULL), szProxyModule, _countof(szProxyModule)))
	{
		fwprintf(stderr, L"[Onlooker] Failed to get module filename.\n");
		return EXIT_FAILURE;
	}
	{
		auto dot = wcsrchr(szProxyModule, L'.');
		if (dot)
		{
			std::wstring ext(dot + 1);
			*dot = L'\0';
			wcscat_s(szProxyModule, L"_onlooker.");
			wcscat_s(szProxyModule, ext.c_str());
		}
		else
		{
			wcscat_s(szProxyModule, L"_onlooker");
		}
	}
	bool proxyMode = FileExists(szProxyModule);
	if (!proxyMode && argc < 2)
	{
		fwprintf(stderr, L"[Onlooker] Usage: Onlooker program [arg1 arg2]\n");
		return EXIT_FAILURE;
	}

	PROCESS_INFORMATION pi = { 0 };
	if (!proxyMode && argc > 2 && _wcsicmp(argv[1], L":attach") == 0)
	{
		pi.dwProcessId = _wtoi(argv[2]);
		pi.hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId);
		if (!pi.hProcess)
		{
			fwprintf(stderr, L"[Onlooker] Failed to attach to pid %u (0x%X)\n", pi.dwProcessId, pi.dwProcessId);
			return EXIT_FAILURE;
		}
	}
	else
	{
		auto cmdLine = GetCommandLineW();
		fwprintf(stderr, L"[Onlooker] Input command line: %s\n", cmdLine);
		auto start = cmdLine;
		if (*start == L'\"')
		{
			do
			{
				start++;
				if (!*start)
					__debugbreak();
			} while (*start != '\"');
			start++; // skip "
		}
		else
		{
			while (*start != ' ' && *start != '\0')
			{
				start++;
			}
		}
		while (*start == ' ')
		{
			start++;
		}

		if (proxyMode)
		{
			szCommandLine[0] = L'\"';
			wcscat_s(szCommandLine, szProxyModule);
			wcscat_s(szCommandLine, L"\" ");
			wcscat_s(szCommandLine, start);
		}
		else
		{
			wcscpy_s(szCommandLine, start);
		}

		fwprintf(stderr, L"[Onlooker] CreateProcess command line (proxy %s): %s\n", proxyMode ? L"enabled" : L"disabled", szCommandLine);
		STARTUPINFOW si = { sizeof(STARTUPINFOW) };
		if (!CreateProcessW(NULL, szCommandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		{
			auto lastError = GetLastError();
			wchar_t formatted[512];
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lastError, MAKELANGID(SUBLANG_NEUTRAL, LANG_NEUTRAL), formatted, _countof(formatted), nullptr);
			auto idx = wcslen(formatted);
			for (idx = wcslen(formatted); idx != 0 && (formatted[idx - 1] == ' ' || formatted[idx - 1] == '\r' || formatted[idx - 1] == '\n'); idx--)
				formatted[idx - 1] = '\0';
			fwprintf(stderr, L"[Onlooker] CreateProcess failed: %s (%d)\n", formatted, lastError);
			return EXIT_FAILURE;
		}
	}
	fwprintf(stderr, L"[Onlooker] Observing PID %u (0x%X)\n", pi.dwProcessId, pi.dwProcessId);
	HANDLE hMonitoringThread = CreateThread(NULL, 0, MonitoringThread, &pi, 0, NULL);
	WaitForSingleObject(pi.hProcess, INFINITE);
	bStopMonitoringThread = true;
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	fwprintf(stderr, L"[OnLooker] Exit code: %d (0x%08X)\n", exitCode, exitCode);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	WaitForSingleObject(hMonitoringThread, INFINITE);
	CloseHandle(hMonitoringThread);
	return exitCode;
}