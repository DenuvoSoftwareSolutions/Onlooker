#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <queue>

#include "regexmagic.h"

struct UniqueProcess
{
	std::string name;
	uint32_t pid = 0;
	uint32_t ppid = 0;
};

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		puts("Usage: Reproduction Onlooker_XXX.log");
		return EXIT_FAILURE;
	}

	std::vector<std::vector<UniqueProcess>> processTimeline;

	std::ifstream ifs(argv[1]);
	std::string line;
	bool inlist = false;
	std::regex r(R"/(^  "([^"]*)" \(PID: (\d+), Parent: (\d+)\)$)/");
	while (std::getline(ifs, line))
	{
		if (line.empty())
			continue;
		if (inlist)
		{
			if (line.find("  ") == 0)
			{
				UniqueProcess p;
				if (match(r, line, p.name, p.pid, p.ppid))
				{
					//printf("Name: '%s', pid: %u, ppid: %u\n", p.name.c_str(), p.pid, p.ppid);
					processTimeline.back().push_back(p);
				}
				else
				{
					// Something went wrong, all lines should match
					printf("Assumption violated for line: '%s'\n", line.c_str());
					return EXIT_FAILURE;
				}
			}
			else
			{
				inlist = false;
				//puts("\nend\n");
			}
		}
		else if (line == "Updated process list:")
		{
			inlist = true;
			processTimeline.emplace_back();
			//puts("yay!");
		}
	}

	uint32_t onlookerPid = 21076;
	uint32_t monitoredPid = 3560;

	for (const auto& processList : processTimeline)
	{
		std::map<uint32_t, UniqueProcess> processes;
		for (const auto& process : processList)
			processes.emplace(process.pid, process);
		std::map<uint32_t, std::vector<uint32_t>> tree; // parentpid -> [childpids]
		for (const auto& process : processList)
			tree[process.ppid].push_back(process.pid);
		// Breath first search, starting from the monitored pid
		if (processes.count(monitoredPid))
		{
			// BFS
			std::queue<uint32_t> queue;
			std::set<uint32_t> visited;
			visited.insert(onlookerPid);
			queue.push(monitoredPid);
			while (!queue.empty())
			{
				uint32_t pid = queue.front();
				queue.pop();
				if (visited.count(pid))
					continue;
				visited.insert(pid);
				for (auto childPid : tree[pid])
					queue.push(childPid);
				if (processes.count(pid))
				{
					auto& p = processes.at(pid);
					printf("%s %u %u\n", p.name.c_str(), p.pid, p.ppid);
				}
			}
		}
		puts("");
	}

	return EXIT_SUCCESS;
}