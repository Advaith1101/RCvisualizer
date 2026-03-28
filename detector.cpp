#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace std;

string SHARED_FILE = "shared_resource.txt";
int    MONITOR_MS  = 100;

mutex out_mutex;

void log_json(const string& event, const string& detail) {
    lock_guard<mutex> lk(out_mutex);
    auto now = chrono::system_clock::now().time_since_epoch();
    auto ns  = chrono::duration_cast<chrono::nanoseconds>(now).count();
    cout << "{\"event\":\""  << event  << "\","
         << "\"detail\":\"" << detail << "\","
         << "\"ts\":"       << ns     << "}"
         << endl;
}

vector<int> get_locking_pids(const string& filepath) {
    vector<int> pids;
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) return pids;

    ifstream locks("/proc/locks");
    string line;
    while (getline(locks, line)) {
        istringstream iss(line);
        string seq, cls, type, pid_s, rest, inode_s;
        iss >> seq >> cls >> type >> pid_s >> rest;
        auto colon2 = rest.rfind(':');
        if (colon2 != string::npos) {
            inode_s = rest.substr(colon2 + 1);
            try {
                ino_t inode = stoul(inode_s);
                if (inode == st.st_ino) {
                    int pid = stoi(pid_s);
                    pids.push_back(pid);
                }
            } catch (...) {}
        }
    }
    return pids;
}

bool try_exclusive_lock(const string& filepath) {
    int fd = open(filepath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return false;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    bool got = (fcntl(fd, F_SETLK, &fl) == 0);
    if (got) {
        fl.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &fl);
    }
    close(fd);
    return got;
}

int main(int argc, char* argv[]) {
    if (argc > 1) SHARED_FILE = argv[1];

    log_json("DETECTOR_START", "monitoring " + SHARED_FILE);

    int  last_conflict_count = 0;
    bool last_locked = false;

    while (true) {
        auto pids      = get_locking_pids(SHARED_FILE);
        bool is_locked = !try_exclusive_lock(SHARED_FILE);

        if (pids.size() > 1) {
            string detail = to_string(pids.size()) + " pids holding locks";
            log_json("MULTI_LOCK", detail);
            last_conflict_count++;
        }

        if (is_locked != last_locked) {
            log_json(is_locked ? "FILE_LOCKED" : "FILE_FREE",
                     is_locked ? "exclusive lock detected" : "file is free");
            last_locked = is_locked;
        }

        this_thread::sleep_for(chrono::milliseconds(MONITOR_MS));
    }

    return 0;
}
