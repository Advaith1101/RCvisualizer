#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <set>

using namespace std;

struct Event {
    long long ts;
    int       tid;
    string    state;
    int       before;
    int       after;
    string    note;
};

string extract_field(const string& json, const string& key) {
    string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == string::npos) return "";
    pos += search.size();
    if (json[pos] == '"') {
        auto end = json.find('"', pos + 1);
        return json.substr(pos + 1, end - pos - 1);
    } else {
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    }
}

Event parse_event(const string& line) {
    Event e;
    try {
        e.ts    = stoll(extract_field(line, "ts"));
        e.tid   = stoi(extract_field(line, "tid"));
        e.state = extract_field(line, "state");
        e.before = stoi(extract_field(line, "before"));
        e.after  = stoi(extract_field(line, "after"));
        e.note   = extract_field(line, "note");
    } catch (...) {}
    return e;
}

void print_ascii_timeline(const vector<Event>& events, int num_threads) {
    long long t0   = events.empty() ? 0 : events[0].ts;
    long long tmax = events.empty() ? 1 : events.back().ts - t0;

    const int WIDTH = 60;
    cout << "\n=== ASCII TIMELINE ===\n";
    cout << setw(10) << "Thread" << " | ";
    cout << left << setw(WIDTH) << "Timeline" << " | Events\n";
    cout << string(10 + 3 + WIDTH + 3 + 20, '-') << "\n";

    for (int tid = 0; tid < num_threads; ++tid) {
        vector<pair<int,string>> marks;

        for (const auto& e : events) {
            if (e.tid != tid) continue;
            long long rel = e.ts - t0;
            int pos = (tmax > 0) ? (int)((double)rel / tmax * (WIDTH - 1)) : 0;
            pos = max(0, min(pos, WIDTH - 1));

            string sym = ".";
            if      (e.state == "WRITE")        sym = (e.note.find("RACE") != string::npos) ? "X" : "W";
            else if (e.state == "READ")         sym = "R";
            else if (e.state == "LOCKED")       sym = "L";
            else if (e.state == "UNLOCKED")     sym = "U";
            else if (e.state == "SEM_ACQUIRED") sym = "S";
            else if (e.state == "SEM_RELEASED") sym = "s";
            else if (e.state == "DONE")         sym = "D";
            marks.push_back({pos, sym});
        }

        string bar(WIDTH, '-');
        for (auto& [pos, sym] : marks) bar[pos] = sym[0];

        cout << right << setw(8) << ("T" + to_string(tid)) << " | ";
        cout << bar << " | ";

        int writes = 0, races = 0;
        for (const auto& e : events) {
            if (e.tid != tid) continue;
            if (e.state == "WRITE") writes++;
            if (e.note.find("RACE") != string::npos) races++;
        }
        cout << "W:" << writes << " R:" << races << "\n";
    }

    cout << "\nLegend: R=Read W=Write X=Race L=Lock U=Unlock S=Sem_Acq s=Sem_Rel D=Done\n\n";
}

void print_report(const vector<Event>& events) {
    int races = 0, writes = 0, reads = 0;
    set<int> tids;

    for (const auto& e : events) {
        tids.insert(e.tid);
        if (e.state == "WRITE") writes++;
        if (e.state == "READ")  reads++;
        if (e.note.find("RACE") != string::npos) races++;
    }

    cout << "=== REPORT ===\n";
    cout << "Threads seen  : " << tids.size()    << "\n";
    cout << "Total events  : " << events.size()  << "\n";
    cout << "Total writes  : " << writes         << "\n";
    cout << "Total reads   : " << reads          << "\n";
    cout << "Race events   : " << races          << "\n";

    if (!events.empty()) {
        int final_counter = events.back().after;
        cout << "Final counter : " << final_counter << "\n";
    }

    if (races > 0)
        cout << "\n[!] UNSAFE: Race conditions detected!\n";
    else
        cout << "\n[+] SAFE: No race conditions detected.\n";
}

int main(int argc, char* argv[]) {
    string logfile = (argc > 1) ? argv[1] : "trace.log";

    ifstream in(logfile);
    if (!in) {
        cerr << "Cannot open " << logfile << "\n";
        return 1;
    }

    vector<Event> events;
    string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] != '{') continue;
        events.push_back(parse_event(line));
    }

    if (events.empty()) {
        cout << "No events found in " << logfile << "\n";
        return 0;
    }

    int max_tid = 0;
    for (const auto& e : events) max_tid = max(max_tid, e.tid);

    print_ascii_timeline(events, max_tid + 1);
    print_report(events);

    return 0;
}
