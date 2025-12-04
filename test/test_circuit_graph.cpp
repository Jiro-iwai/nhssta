// -*- c++ -*-
// Unit tests for CircuitGraph class
// Phase 2: Graph construction separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <functional>

#include "../src/circuit_graph.hpp"
#include "../src/dlib_parser.hpp"
#include "../src/bench_parser.hpp"
#include <nhssta/gate.hpp>
#include "../src/normal.hpp"

using Nh::CircuitGraph;
using Nh::DlibParser;
using Nh::BenchParser;
using Nh::Gate;
using Normal = RandomVariable::Normal;
using RandomVar = RandomVariable::RandomVariable;

class CircuitGraphTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        // Create test directory if it doesn't exist
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
        
        // Create test dlib file
        std::string dlib_content = 
            "inv  0  y  gauss  (15.0, 2.0)\n"
            "nand  0  y  gauss  (24.0, 3.0)\n"
            "nand  1  y  gauss  (20.0, 3.0)\n";
        dlib_path = createTestFile("test_circuit.dlib", dlib_content);
    }

    void TearDown() override {
        deleteTestFile("test_circuit.dlib");
    }

    std::string createTestFile(const std::string& filename, const std::string& content) {
        std::string filepath = test_dir + "/" + filename;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
        }
        return filepath;
    }

    void deleteTestFile(const std::string& filename) {
        std::string filepath = test_dir + "/" + filename;
        remove(filepath.c_str());
    }

    std::string test_dir;
    std::string dlib_path;
};

// Test: CircuitGraph with simple circuit
TEST_F(CircuitGraphTest, BuildSimpleCircuit) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_simple.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Build circuit graph
    CircuitGraph graph;
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    const auto& signals = graph.signals();
    EXPECT_EQ(signals.size(), 2);  // A (input) and Y (output)
    EXPECT_NE(signals.find("A"), signals.end());
    EXPECT_NE(signals.find("Y"), signals.end());
    
    // Check Y signal has correct timing
    auto y_it = signals.find("Y");
    EXPECT_NE(y_it, signals.end());
    const RandomVar& y_signal = y_it->second;
    EXPECT_GT(y_signal->mean(), 0.0);
    
    deleteTestFile("test_simple.bench");
}

// Test: CircuitGraph with multiple gates
TEST_F(CircuitGraphTest, BuildMultipleGates) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "OUTPUT(Y)\n"
        "N1 = INV(A)\n"
        "N2 = INV(B)\n"
        "Y = NAND(N1, N2)\n";
    std::string bench_path = createTestFile("test_multiple.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Build circuit graph
    CircuitGraph graph;
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    const auto& signals = graph.signals();
    EXPECT_EQ(signals.size(), 5);  // A, B, N1, N2, Y
    EXPECT_NE(signals.find("A"), signals.end());
    EXPECT_NE(signals.find("B"), signals.end());
    EXPECT_NE(signals.find("N1"), signals.end());
    EXPECT_NE(signals.find("N2"), signals.end());
    EXPECT_NE(signals.find("Y"), signals.end());
    
    deleteTestFile("test_multiple.bench");
}

// Test: CircuitGraph with track_path callback
TEST_F(CircuitGraphTest, BuildWithTrackPathCallback) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_callback.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Track path callback
    std::vector<std::string> tracked_signals;
    std::vector<std::string> tracked_gates;
    
    CircuitGraph::TrackPathCallback callback = 
        [&](const std::string& signal_name, const Nh::Instance& inst, 
            const Nh::NetLineIns& ins, const std::string& gate_type) {
            tracked_signals.push_back(signal_name);
            tracked_gates.push_back(gate_type);
        };
    
    // Build circuit graph
    CircuitGraph graph;
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs(),
                callback);
    
    // Check callback was called
    EXPECT_EQ(tracked_signals.size(), 1);
    EXPECT_EQ(tracked_signals[0], "Y");
    EXPECT_EQ(tracked_gates.size(), 1);
    EXPECT_EQ(tracked_gates[0], "inv");
    
    deleteTestFile("test_callback.bench");
}

// Test: CircuitGraph with DFF
TEST_F(CircuitGraphTest, BuildWithDFF) {
    // Parse dlib with DFF
    std::string dlib_content = 
        "inv  0  y  gauss  (15.0, 2.0)\n"
        "dff  ck  q  gauss  (10.0, 1.0)\n";
    std::string dlib_path_dff = createTestFile("test_dff_circuit.dlib", dlib_content);
    DlibParser dlib_parser(dlib_path_dff);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with DFF
    std::string bench_content = 
        "INPUT(CLK)\n"
        "INPUT(D)\n"
        "OUTPUT(Q)\n"
        "Q = DFF(D, CLK)\n";
    std::string bench_path = createTestFile("test_dff.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Build circuit graph
    CircuitGraph graph;
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs());
    
    const auto& signals = graph.signals();
    EXPECT_NE(signals.find("Q"), signals.end());
    
    deleteTestFile("test_dff_circuit.dlib");
    deleteTestFile("test_dff.bench");
}

// Test: CircuitGraph with unknown gate (should throw)
TEST_F(CircuitGraphTest, BuildWithUnknownGate) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with unknown gate - should throw ParseException during parsing
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = UNKNOWN(A)\n";
    std::string bench_path = createTestFile("test_unknown.bench", bench_content);
    BenchParser bench_parser(bench_path);
    
    // Parse should throw ParseException for unknown gate
    EXPECT_THROW({
        bench_parser.parse(gates);
    }, Nh::ParseException);
    
    deleteTestFile("test_unknown.bench");
}

// Test: CircuitGraph with floating node (should throw)
TEST_F(CircuitGraphTest, BuildWithFloatingNode) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench with floating node (Y depends on undefined signal Z)
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(Z)\n";  // Z is not defined
    std::string bench_path = createTestFile("test_floating.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Build circuit graph should throw
    CircuitGraph graph;
    EXPECT_THROW({
        graph.build(gates, bench_parser.net(), 
                    bench_parser.inputs(), bench_parser.outputs(),
                    bench_parser.dff_outputs(), bench_parser.dff_inputs());
    }, Nh::RuntimeException);
    
    deleteTestFile("test_floating.bench");
}

// Test: CircuitGraph path tracking data structures
TEST_F(CircuitGraphTest, PathTrackingDataStructures) {
    // Parse dlib
    DlibParser dlib_parser(dlib_path);
    dlib_parser.parse();
    const auto& gates = dlib_parser.gates();
    
    // Parse bench
    std::string bench_content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string bench_path = createTestFile("test_tracking.bench", bench_content);
    BenchParser bench_parser(bench_path);
    bench_parser.parse(gates);
    
    // Track path callback
    CircuitGraph::TrackPathCallback callback = 
        [](const std::string& signal_name, const Nh::Instance& inst, 
           const Nh::NetLineIns& ins, const std::string& gate_type) {
            // Do nothing, just track
        };
    
    // Build circuit graph
    CircuitGraph graph;
    graph.build(gates, bench_parser.net(), 
                bench_parser.inputs(), bench_parser.outputs(),
                bench_parser.dff_outputs(), bench_parser.dff_inputs(),
                callback);
    
    // Check path tracking data structures
    const auto& signal_to_instance = graph.signal_to_instance();
    const auto& instance_to_inputs = graph.instance_to_inputs();
    const auto& instance_to_gate_type = graph.instance_to_gate_type();
    const auto& instance_to_delays = graph.instance_to_delays();
    
    EXPECT_EQ(signal_to_instance.size(), 1);
    EXPECT_NE(signal_to_instance.find("Y"), signal_to_instance.end());
    
    EXPECT_EQ(instance_to_inputs.size(), 1);
    EXPECT_EQ(instance_to_gate_type.size(), 1);
    EXPECT_EQ(instance_to_delays.size(), 1);
    
    deleteTestFile("test_tracking.bench");
}

