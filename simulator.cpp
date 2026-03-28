#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>

using namespace std;

int    NUM_THREADS = 4;
int    SPEED_MS    = 300;
int    ITERATIONS  = 5;
int    MODE        = 0;
string LOG_FILE    = "trace.log";

int    shared_counter = 0;

mutex  mtx;
sem_t  sem;

mutex    log_mutex;
ofstream log_file;

string now_ns() {
    auto now = chrono::system_clock::now();
    auto ns  = chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();
    return to_string(ns);
}

void log_event(int tid, const string& state, int counter_before, int counter_after, const string& note = "") {
    lock_guard<mutex> lk(log_mutex);
    log_file << "{"
             << "\"ts\":"     << now_ns()       << ","
             << "\"tid\":"    << tid             << ","
             << "\"state\":\"" << state          << "\","
             << "\"before\":" << counter_before  << ","
             << "\"after\":"  << counter_after   << ","
             << "\"note\":\""  << note           << "\""
             << "}\n";
    log_file.flush();

    cout << "{"
         << "\"ts\":"     << now_ns()      << ","
         << "\"tid\":"    << tid            << ","
         << "\"state\":\"" << state         << "\","
         << "\"before\":" << counter_before << ","
         << "\"after\":"  << counter_after  << ","
         << "\"note\":\""  << note          << "\""
         << "}" << endl;
}

void thread_unsafe(int tid) {
    for (int i = 0; i < ITERATIONS; ++i) {
        log_event(tid, "READ", shared_counter, shared_counter, "reading counter");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));

        int local = shared_counter;
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS / 2));
        local++;
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS / 2));

        int before = shared_counter;
        shared_counter = local;
        log_event(tid, "WRITE", before, shared_counter, before != local - 1 ? "RACE DETECTED" : "ok");

        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));
    }
    log_event(tid, "DONE", shared_counter, shared_counter, "thread finished");
}

void thread_mutex(int tid) {
    for (int i = 0; i < ITERATIONS; ++i) {
        log_event(tid, "WAIT_LOCK", shared_counter, shared_counter, "waiting for mutex");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS / 4));

        mtx.lock();
        log_event(tid, "LOCKED", shared_counter, shared_counter, "acquired mutex");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));

        int before = shared_counter;
        shared_counter++;
        log_event(tid, "WRITE", before, shared_counter, "safe write");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS / 2));

        mtx.unlock();
        log_event(tid, "UNLOCKED", shared_counter, shared_counter, "released mutex");

        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));
    }
    log_event(tid, "DONE", shared_counter, shared_counter, "thread finished");
}

void thread_semaphore(int tid) {
    for (int i = 0; i < ITERATIONS; ++i) {
        log_event(tid, "WAIT_SEM", shared_counter, shared_counter, "waiting on semaphore");

        sem_wait(&sem);
        log_event(tid, "SEM_ACQUIRED", shared_counter, shared_counter, "semaphore acquired");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));

        int before = shared_counter;
        shared_counter++;
        log_event(tid, "WRITE", before, shared_counter, "safe write via semaphore");
        this_thread::sleep_for(chrono::milliseconds(SPEED_MS / 2));

        sem_post(&sem);
        log_event(tid, "SEM_RELEASED", shared_counter, shared_counter, "semaphore released");

        this_thread::sleep_for(chrono::milliseconds(SPEED_MS));
    }
    log_event(tid, "DONE", shared_counter, shared_counter, "thread finished");
}

int main(int argc, char* argv[]) {
    if (argc > 1) NUM_THREADS = stoi(argv[1]);
    if (argc > 2) SPEED_MS    = stoi(argv[2]);
    if (argc > 3) MODE        = stoi(argv[3]);

    NUM_THREADS = max(1, min(NUM_THREADS, 10));
    SPEED_MS    = max(10, min(SPEED_MS, 2000));
    MODE        = max(0, min(MODE, 2));

    log_file.open(LOG_FILE, ios::app);

    string mode_name = (MODE == 0) ? "UNSAFE" : (MODE == 1) ? "MUTEX" : "SEMAPHORE";

    cout << "{\"event\":\"START\","
         << "\"mode\":\""   << mode_name   << "\","
         << "\"threads\":"  << NUM_THREADS << ","
         << "\"speed\":"    << SPEED_MS    << ","
         << "\"iterations\":" << ITERATIONS << "}"
         << endl;

    sem_init(&sem, 0, 1);

    vector<thread> threads;

    if (MODE == 0) {
        for (int i = 0; i < NUM_THREADS; ++i)
            threads.emplace_back(thread_unsafe, i);
    } else if (MODE == 1) {
        for (int i = 0; i < NUM_THREADS; ++i)
            threads.emplace_back(thread_mutex, i);
    } else {
        for (int i = 0; i < NUM_THREADS; ++i)
            threads.emplace_back(thread_semaphore, i);
    }

    for (auto& t : threads) t.join();

    int expected = NUM_THREADS * ITERATIONS;
    cout << "{\"event\":\"END\","
         << "\"final_counter\":" << shared_counter << ","
         << "\"expected\":"      << expected        << ","
         << "\"races\":"         << (shared_counter != expected ? "true" : "false") << "}"
         << endl;

    sem_destroy(&sem);
    log_file.close();
    return 0;
}
