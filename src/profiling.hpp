// -*- c++ -*-
// Profiling utilities for performance analysis

#ifndef PROFILING__H
#define PROFILING__H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace Profiling {

class Profiler;

class Timer {
public:
    Timer(const std::string& name) : name_(name), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~Timer();
    
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

class Profiler {
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }
    
    void record(const std::string& name, long microseconds) {
#ifdef DEBUG
        if (enabled_) {
            auto& stats = stats_[name];
            stats.total_time += microseconds;
            stats.call_count++;
            stats.min_time = std::min(stats.min_time, microseconds);
            stats.max_time = std::max(stats.max_time, microseconds);
        }
#else
        (void)name;
        (void)microseconds;
#endif
    }
    
    void enable() {
#ifdef DEBUG
        enabled_ = true;
#endif
    }
    void disable() {
#ifdef DEBUG
        enabled_ = false;
#endif
    }
    void reset() {
#ifdef DEBUG
        stats_.clear();
#endif
    }
    
    void printReport(std::ostream& os = std::cerr) {
#ifdef DEBUG
        if (!enabled_ || stats_.empty()) {
            return;
        }
        
        // Convert to vector and sort by total time
        std::vector<std::pair<std::string, Stats>> sorted_stats;
        for (const auto& pair : stats_) {
            sorted_stats.push_back(pair);
        }
        std::sort(sorted_stats.begin(), sorted_stats.end(),
                  [](const auto& a, const auto& b) {
                      return a.second.total_time > b.second.total_time;
                  });
        
        os << "\n=== Profiling Report ===" << std::endl;
        os << std::left << std::setw(50) << "Function" 
            << std::right << std::setw(12) << "Calls"
            << std::setw(15) << "Total (ms)"
            << std::setw(15) << "Avg (ms)"
            << std::setw(15) << "Min (ms)"
            << std::setw(15) << "Max (ms)" << std::endl;
        os << std::string(120, '-') << std::endl;
        
        long total_all = 0;
        for (const auto& pair : sorted_stats) {
            const auto& stats = pair.second;
            total_all += stats.total_time;
            double total_ms = stats.total_time / 1000.0;
            double avg_ms = stats.total_time / (1000.0 * stats.call_count);
            double min_ms = stats.min_time / 1000.0;
            double max_ms = stats.max_time / 1000.0;
            
            os << std::left << std::setw(50) << pair.first
                << std::right << std::setw(12) << stats.call_count
                << std::setw(15) << std::fixed << std::setprecision(3) << total_ms
                << std::setw(15) << std::fixed << std::setprecision(3) << avg_ms
                << std::setw(15) << std::fixed << std::setprecision(3) << min_ms
                << std::setw(15) << std::fixed << std::setprecision(3) << max_ms << std::endl;
        }
        
        os << std::string(120, '-') << std::endl;
        os << std::left << std::setw(50) << "TOTAL"
            << std::right << std::setw(12) << ""
            << std::setw(15) << std::fixed << std::setprecision(3) << (total_all / 1000.0) << std::endl;
        os << std::endl;
#endif
    }
    
private:
    struct Stats {
        long total_time = 0;
        long call_count = 0;
        long min_time = LONG_MAX;
        long max_time = 0;
    };
    
    std::unordered_map<std::string, Stats> stats_;
    bool enabled_ = false;
    
    Profiler() = default;
    ~Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
};

// Macro for easy profiling (only enabled in DEBUG builds)
#ifdef DEBUG
#define PROFILE_SCOPE(name) Profiling::Timer _timer(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__PRETTY_FUNCTION__)
#else
#define PROFILE_SCOPE(name) ((void)0)
#define PROFILE_FUNCTION() ((void)0)
#endif

// Timer destructor implementation (must be after Profiler definition)
inline Timer::~Timer() {
#ifdef DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    Profiler::getInstance().record(name_, duration.count());
#endif
}

} // namespace Profiling

#endif // PROFILING__H

