#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <aria2/aria2.h>

long getRSS() {
    long rss = 0;
    std::ifstream stat_stream("/proc/self/statm", std::ios_base::in);
    if (stat_stream.is_open()) {
        long pages;
        stat_stream >> pages >> rss;
        stat_stream.close();
    }
    return rss * sysconf(_SC_PAGESIZE);
}

struct CpuTicks {
    long utime;
    long stime;
};

CpuTicks getCpuTicks() {
    CpuTicks ticks = {0, 0};
    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);
    if (stat_stream.is_open()) {
        std::string dummy;
        for (int i = 0; i < 13; ++i) stat_stream >> dummy;
        stat_stream >> ticks.utime >> ticks.stime;
        stat_stream.close();
    }
    return ticks;
}

int main(int argc, char* argv[]) {
    std::string url;
    std::string connections = "16";
    std::string split = "16";

    if (argc >= 2) {
        url = argv[1];
        if (argc >= 3) connections = argv[2];
        if (argc >= 4) split = argv[3];
    } else {
        std::ifstream url_file("url.txt");
        if (!url_file.is_open() || !std::getline(url_file, url)) {
            std::cerr << "Usage: " << argv[0] << " <URL> [connections] [split]" << std::endl;
            std::cerr << "Or provide a URL in url.txt" << std::endl;
            return 1;
        }
    }

    // Phase 1: libaria2 Setup
    if (aria2::libraryInit() != 0) {
        std::cerr << "Failed to initialize libaria2" << std::endl;
        return 1;
    }

    aria2::KeyVals options;
    options.push_back({"max-connection-per-server", connections});
    options.push_back({"split", split});
    options.push_back({"min-split-size", "1M"});
    options.push_back({"disk-cache", "32M"});
    options.push_back({"file-allocation", "falloc"});
    options.push_back({"log", "aria2.log"});
    options.push_back({"log-level", "debug"});

    aria2::SessionConfig config;
    aria2::Session* session = aria2::sessionNew(options, config);
    if (!session) {
        std::cerr << "Failed to create aria2 session" << std::endl;
        aria2::libraryDeinit();
        return 1;
    }

    aria2::A2Gid gid;
    int rv = aria2::addUri(session, &gid, {url}, {});
    if (rv != 0) {
        std::cerr << "Failed to add URI: " << rv << std::endl;
        aria2::sessionFinal(session);
        aria2::libraryDeinit();
        return 1;
    }

    std::cout << "Starting download: " << url << std::endl;
    std::cout << "Connections: " << connections << ", Split: " << split << std::endl;
    std::cout << std::left << std::setw(15) << "Speed" 
              << std::setw(15) << "Progress %" 
              << std::setw(15) << "RAM (MB)" 
              << std::setw(15) << "CPU %" << std::endl;

    auto last_time = std::chrono::steady_clock::now();
    CpuTicks last_ticks = getCpuTicks();
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    while (aria2::run(session, aria2::RUN_ONCE) == 1) {
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = current_time - last_time;

        if (elapsed.count() >= 1.0) {
            aria2::DownloadHandle* handle = aria2::getDownloadHandle(session, gid);
            if (handle) {
                double speed = handle->getDownloadSpeed() / (1024.0 * 1024.0);
                long total = handle->getTotalLength();
                long completed = handle->getCompletedLength();
                double progress = (total > 0) ? (completed * 100.0 / total) : 0.0;
                long rss = getRSS() / (1024 * 1024);

                CpuTicks current_ticks = getCpuTicks();
                double cpu_usage = ((current_ticks.utime + current_ticks.stime) - (last_ticks.utime + last_ticks.stime)) 
                                   / (elapsed.count() * ticks_per_sec) * 100.0;

                std::cout << std::left << std::setprecision(2) << std::fixed
                          << std::setw(15) << (std::to_string(speed) + " MB/s")
                          << std::setw(15) << progress
                          << std::setw(15) << rss
                          << std::setw(15) << cpu_usage << std::endl;

                // Check Guardrails
                if (rss > 500) {
                    std::cerr << "ABORT: RAM usage exceeded 500MB (" << rss << " MB)" << std::endl;
                    aria2::deleteDownloadHandle(handle);
                    break;
                }
                if (cpu_usage > 100.0) {
                    std::cerr << "ABORT: CPU usage exceeded 100% (" << cpu_usage << " %)" << std::endl;
                    aria2::deleteDownloadHandle(handle);
                    break;
                }
/*
                if (progress >= 10.0) {
                    std::cout << "SUCCESS: 10% completion reached." << std::endl;
                    aria2::deleteDownloadHandle(handle);
                    break;
                }
*/

                aria2::deleteDownloadHandle(handle);
                last_time = current_time;
                last_ticks = current_ticks;
            }
        }
    }

    std::cout << "Loop exited. Checking final status..." << std::endl;
    aria2::GlobalStat gstat = aria2::getGlobalStat(session);
    std::cout << "Global stat: " << "Active=" << gstat.numActive 
              << ", Waiting=" << gstat.numWaiting 
              << ", Stopped=" << gstat.numStopped << std::endl;

    aria2::DownloadHandle* final_handle = aria2::getDownloadHandle(session, gid);
    if (final_handle) {
        int errCode = final_handle->getErrorCode();
        double progress = (final_handle->getTotalLength() > 0) ? 
                          (final_handle->getCompletedLength() * 100.0 / final_handle->getTotalLength()) : 0.0;
        std::cout << "Final Progress: " << progress << "%" << std::endl;
        if (errCode != 0) {
            std::cerr << "Download stopped with error code: " << errCode << std::endl;
        }
        aria2::deleteDownloadHandle(final_handle);
    }

    aria2::sessionFinal(session);
    aria2::libraryDeinit();
    return 0;
}
