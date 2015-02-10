#pragma once

#include <functional>
#include <vector>

#include "common.hpp"

class TPath;

int RetryBusy(int times, int timeoMs, std::function<int()> handler);
int RetryFailed(int times, int timeoMs, std::function<int()> handler);
int SleepWhile(int timeoMs, std::function<int()> handler);
int GetPid();
size_t GetCurrentTimeMs();
size_t GetTotalMemory();
int CreatePidFile(const std::string &path, const int mode);
void RemovePidFile(const std::string &path);
void SetProcessName(const std::string &name);
void SetDieOnParentExit();
std::string GetProcessName();
TError GetTaskCgroups(const int pid, std::map<std::string, std::string> &cgmap);
std::string GetHostName();
TError SetHostName(const std::string &name);
bool FdHasEvent(int fd);
TError DropBoundedCap(int cap);
TError SetCap(uint64_t effective, uint64_t permitted, uint64_t inheritable);
void CloseFds(int max, const std::vector<int> &except);

class TScopedFd : public TNonCopyable {
    int Fd;
public:
    TScopedFd(int fd = -1);
    ~TScopedFd();
    int GetFd();
    TScopedFd &operator=(int fd);
};

TError SetOomScoreAdj(int value);

int64_t GetBootTime();
TError Run(const std::vector<std::string> &command, int &status);
TError AllocLoop(const TPath &path, size_t size);
TError Popen(const std::string &cmd, std::vector<std::string> &lines);
