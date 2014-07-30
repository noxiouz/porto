#ifndef __FOLDER_H__
#define __FOLDER_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <unistd.h>

#include "log.hpp"

class TFile {
    string path;

public:
    enum EFileType {
        Regular,
        Directory,
        Block,
        Character,
        Fifo,
        Link,
        Socket,
        Unknown
    };    

    TFile(string path) : path(path) {};

    EFileType Type() {
        struct stat st;

        if (lstat(path.c_str(), &st))
            throw "Cannot stat: " + path;

        if (S_ISREG(st.st_mode))
            return Regular;
        else if (S_ISDIR(st.st_mode))
            return Directory;
        else if (S_ISCHR(st.st_mode))
            return Character;
        else if (S_ISBLK(st.st_mode))
            return Block;
        else if (S_ISFIFO(st.st_mode))
            return Fifo;
        else if (S_ISLNK(st.st_mode))
            return Link;
        else if (S_ISSOCK(st.st_mode))
            return Socket;
        else
            return Unknown;
    }
    
    void Remove() {
        int ret = unlink(path.c_str());
        TLogger::LogAction("unlink " + path, ret, errno);

        if (ret && (errno != ENOENT))
            throw "Cannot delete file: " + path;
    }
};

class TFolder {
    string path;

public:
    TFolder(string path) : path(path) {}

    void Create(mode_t mode = 0x755) {
        int ret = mkdir(path.c_str(), mode);

        TLogger::LogAction("mkdir " + path, ret, errno);

        if (ret) {
            switch (errno) {
            case EEXIST:
                throw "Folder already exists";
            default:
                throw "Cannot create folder: " + string(strerror(errno));
            }
        }
    }

    void Remove(bool recursive = false) {
        if (recursive) {
            for (auto f : Items()) {
                TFile child(f);

                if (child.Type() == TFile::Directory)
                    TFolder(f).Remove(recursive);
                else
                    child.Remove();
            }
        }

        int ret = rmdir(path.c_str());

        TLogger::LogAction("rmdir " + path, ret, errno);

        if (ret)
            throw "Cannot remove folder: " + path;
    }

    bool Exists() {
        struct stat st;

        int ret = stat(path.c_str(), &st);

        if (ret == 0 && S_ISDIR(st.st_mode))
            return true;
        else if (ret && errno == ENOENT)
            return false;
        else
            throw "Cannot stat folder: " + path;
    }

    vector<string> Subfolders() {
        return Items(true, true);
    }

    vector<string> Items(bool only_folders = false, bool skip_links = true) {
        DIR *dirp;
        struct dirent dp, *res;
        vector<string> ret;

        dirp = opendir(path.c_str());
        if (!dirp)
            throw "Cannot open folder " + path;
        
        while (!readdir_r(dirp, &dp, &res) && res != nullptr) {
            if (!strcmp(".", res->d_name) || !strcmp ("..", res->d_name))
                continue;
            if (only_folders && !(res->d_type == DT_DIR))
                continue;
            if (skip_links && (res->d_type == DT_LNK))
                continue;

            ret.push_back(path + "/" + string(res->d_name));
        }

        closedir(dirp);
        return ret;
    }
};

#endif
