// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef NH_SSTA__H
#define NH_SSTA__H

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
// smart_ptr.hpp no longer needed - NetLineImpl uses std::shared_ptr directly
#include <nhssta/exception.hpp>
#include <nhssta/ssta_results.hpp>

#include <nhssta/gate.hpp>
#include <nhssta/net_line.hpp>

namespace Nh {

// Forward declarations for refactored components (Phase 5)
class DlibParser;
class BenchParser;
class CircuitGraph;
class CriticalPathAnalyzer;
class SensitivityAnalyzer;
class SstaResults;

// Default number of critical paths to report when -p/--path option
// is specified without an explicit count.
constexpr size_t DEFAULT_CRITICAL_PATH_COUNT = 5;

class Ssta {
   public:

    Ssta();
    ~Ssta();
    void check();
    void read_dlib();
    void read_bench();

   private:
    // Old methods removed - functionality moved to refactored classes (Phase 5)

    ////

    // Use unordered_map for better performance (O(1) average vs O(log n) for map)
    using Gates = std::unordered_map<std::string, Gate>;
    using Net = std::list<NetLine>;
    using Pins = std::set<std::string>;

    std::string dlib_;
    std::string bench_;
    bool is_lat_ = false;
    bool is_correlation_ = false;
    bool is_critical_path_ = false;
    bool is_sensitivity_ = false;
    size_t critical_path_count_ = DEFAULT_CRITICAL_PATH_COUNT;
    size_t sensitivity_top_n_ = DEFAULT_CRITICAL_PATH_COUNT;
    
    // Refactored components (Phase 5)
    std::unique_ptr<DlibParser> dlib_parser_;
    std::unique_ptr<BenchParser> bench_parser_;
    std::unique_ptr<CircuitGraph> circuit_graph_;
    std::unique_ptr<CriticalPathAnalyzer> path_analyzer_;
    std::unique_ptr<SensitivityAnalyzer> sensitivity_analyzer_;
    std::unique_ptr<SstaResults> results_;
    
    // Gatesは感度解析が有効な場合のみ保持（メモリ最適化）
    Gates gates_;  // DlibParserから取得後、感度解析が無効ならクリア

   public:
    void set_lat() {
        is_lat_ = true;
    }
    void set_correlation() {
        is_correlation_ = true;
    }
    void set_critical_path(size_t top_n = DEFAULT_CRITICAL_PATH_COUNT) {
        is_critical_path_ = true;
        critical_path_count_ = top_n;
    }
    void set_sensitivity() {
        is_sensitivity_ = true;
    }
    void set_sensitivity_top_n(size_t top_n) {
        sensitivity_top_n_ = top_n;
    }

    [[nodiscard]] bool is_lat() const {
        return is_lat_;
    }
    [[nodiscard]] bool is_correlation() const {
        return is_correlation_;
    }
    [[nodiscard]] bool is_critical_path() const {
        return is_critical_path_;
    }
    [[nodiscard]] bool is_sensitivity() const {
        return is_sensitivity_;
    }

    void set_dlib(const std::string& dlib) {
        dlib_ = dlib;
    }
    void set_bench(const std::string& bench) {
        bench_ = bench;
    }

    // Pure logic functions (Phase 2: I/O separation)
    [[nodiscard]] LatResults getLatResults() const;
    [[nodiscard]] CorrelationMatrix getCorrelationMatrix() const;
    [[nodiscard]] CriticalPaths getCriticalPaths(size_t top_n) const;
    [[nodiscard]] CriticalPaths getCriticalPaths() const {
        return getCriticalPaths(critical_path_count_);
    }
    [[nodiscard]] SensitivityResults getSensitivityResults(size_t top_n) const;
    [[nodiscard]] SensitivityResults getSensitivityResults() const {
        return getSensitivityResults(sensitivity_top_n_);
    }
};
}  // namespace Nh

#endif  // NH_SSTA__H
