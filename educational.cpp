\#include <iostream>
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>
using namespace std;
// colours
#define RESET   "\033[0m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"
#define DIM     "\033[2m"

void separator(const string& title = "") {
    cout << "\n" << DIM << string(60, '-') << RESET << "\n";
    if (!title.empty())
        cout << WHITE << "  " << title << RESET << "\n"
             << DIM << string(60, '-') << RESET << "\n";
}

void pause_for(int ms) {
    this_thread::sleep_for(chrono::milliseconds(ms));
}

void print_shared(int expected, int actual) {
    bool ok = actual == expected;
    cout << "  Shared counter : "
         << (ok ? GREEN : RED)
         << setw(4) << actual << RESET
         << "  (expected " << expected << ")"
         << (ok ? "" : RED "  <- CORRUPTED!" RESET)
         << "\n";
}

int unsafe_counter = 0;

void unsafe_thread(int tid, int iters, int delay_ms) {
    for (int i = 0; i < iters; ++i) {
        int local = unsafe_counter;
        pause_for(delay_ms + (tid * 5));
        local++;
        unsafe_counter = local;
    }
}

void demo_unsafe() {
    separator("DEMO 1: No Synchronization (Race Conditions)");
    cout << YELLOW <<
        "  What happens when 4 threads increment a counter 5 times each\n"
        "  with NO synchronization?\n\n"
        "  Expected final value: 4 x 5 = 20\n" << RESET;
    pause_for(500);

    unsafe_counter = 0;
    int N = 4, iters = 5, delay = 50;
    vector<thread> threads;

    cout << "\n  " DIM "Starting 4 threads..." RESET "\n";
    for (int i = 0; i < N; ++i)
        threads.emplace_back(unsafe_thread, i, iters, delay);
    for (auto& t : threads) t.join();

    cout << "\n";
    print_shared(N * iters, unsafe_counter);

    if (unsafe_counter != N * iters) {
        cout << "\n" RED
            "  [!] Race condition occurred!\n"
            "      The READ-INCREMENT-WRITE sequence was interrupted.\n"
            "      When two threads read the same value and both write back +1,\n"
            "      one increment is lost. This is a LOST UPDATE.\n" RESET;
    }
}

int mutex_counter = 0;
mutex mtx_demo;

void mutex_thread(int tid, int iters, int delay_ms) {
    for (int i = 0; i < iters; ++i) {
        mtx_demo.lock();
        int local = mutex_counter;
        pause_for(delay_ms);
        local++;
        mutex_counter = local;
        mtx_demo.unlock();
    }
}

void demo_mutex() {
    separator("DEMO 2: Mutex (Mutual Exclusion)");
    cout << CYAN <<
        "  std::mutex ensures only ONE thread is in the critical section at a time.\n"
        "  Other threads BLOCK at mtx.lock() and wait their turn.\n\n"
        "  Expected final value: 4 x 5 = 20\n" << RESET;
    pause_for(500);

    mutex_counter = 0;
    int N = 4, iters = 5, delay = 50;
    vector<thread> threads;

    cout << "\n  " DIM "Starting 4 threads with mutex protection..." RESET "\n";
    for (int i = 0; i < N; ++i)
        threads.emplace_back(mutex_thread, i, iters, delay);
    for (auto& t : threads) t.join();

    cout << "\n";
    print_shared(N * iters, mutex_counter);
    cout << GREEN "  [+] No race conditions. Mutex serialized all writes.\n" RESET;
}

int sem_counter = 0;
sem_t sem_demo;

void semaphore_thread(int tid, int iters, int delay_ms) {
    for (int i = 0; i < iters; ++i) {
        sem_wait(&sem_demo);
        int local = sem_counter;
        pause_for(delay_ms);
        local++;
        sem_counter = local;
        sem_post(&sem_demo);
    }
}

void demo_semaphore() {
    separator("DEMO 3: Semaphore");
    cout << MAGENTA <<
        "  POSIX sem_t (binary, init=1) works like a mutex here.\n"
        "  sem_wait() = P() = decrement and possibly block\n"
        "  sem_post() = V() = increment and possibly wake a thread\n\n"
        "  Counting semaphores (init=N) allow N concurrent accesses.\n\n"
        "  Expected final value: 4 x 5 = 20\n" << RESET;
    pause_for(500);

    sem_counter = 0;
    sem_init(&sem_demo, 0, 1);

    int N = 4, iters = 5, delay = 50;
    vector<thread> threads;

    cout << "\n  " DIM "Starting 4 threads with semaphore protection..." RESET "\n";
    for (int i = 0; i < N; ++i)
        threads.emplace_back(semaphore_thread, i, iters, delay);
    for (auto& t : threads) t.join();

    cout << "\n";
    print_shared(N * iters, sem_counter);
    cout << GREEN "  [+] No race conditions. Semaphore serialized all writes.\n" RESET;

    sem_destroy(&sem_demo);
}

int pool_access_count = 0;
mutex pool_log_mtx;
sem_t pool_sem;

void pool_thread(int tid) {
    sem_wait(&pool_sem);
    {
        lock_guard<mutex> lk(pool_log_mtx);
        pool_access_count++;
        cout << "  T" << tid << " entered   (concurrent: " << pool_access_count << ")\n";
    }
    pause_for(80);
    {
        lock_guard<mutex> lk(pool_log_mtx);
        pool_access_count--;
        cout << "  T" << tid << " exited    (concurrent: " << pool_access_count << ")\n";
    }
    sem_post(&pool_sem);
}

void demo_counting_semaphore() {
    separator("DEMO 4: Counting Semaphore (N=3 concurrent)");
    cout << BLUE <<
        "  A counting semaphore with init=3 allows 3 threads simultaneously.\n"
        "  Models a resource pool — e.g., 3 database connections.\n" << RESET << "\n";
    pause_for(300);

    sem_init(&pool_sem, 0, 3);
    int N = 8;
    vector<thread> threads;
    for (int i = 0; i < N; ++i)
        threads.emplace_back(pool_thread, i);
    for (auto& t : threads) t.join();
    sem_destroy(&pool_sem);

    cout << GREEN "\n  [+] Never more than 3 threads accessed simultaneously.\n" RESET;
}

int main() {
    cout << WHITE "\n+----------------------------------------------+\n"
                  "|   Race Condition Visualizer - Education     |\n"
                  "+----------------------------------------------+\n" RESET;

    demo_unsafe();
    cout << "\n  Press ENTER to continue..."; cin.get();

    demo_mutex();
    cout << "\n  Press ENTER to continue..."; cin.get();

    demo_semaphore();
    cout << "\n  Press ENTER to continue..."; cin.get();

    demo_counting_semaphore();

    separator("Summary");
    cout <<
        YELLOW "  Unsafe    " RESET "-> Fast, but counter gets corrupted by races\n"
        CYAN   "  Mutex     " RESET "-> One thread at a time; simple, correct\n"
        MAGENTA"  Semaphore " RESET "-> Like mutex, but can allow N concurrent accesses\n"
        "\n";

    return 0;
}
