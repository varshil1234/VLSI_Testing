#include <iostream>
#include <chrono> // NEW: For high-resolution execution timing
#include "Circuit.hpp"

int main() {
    CircuitAnalyzer analyzer;
    
    std::string filename = "circuit.v"; 
    
    std::cout << "1. Parsing structural Verilog from " << filename << "...\n";
    analyzer.parseVerilog(filename);
    
    std::cout << "2. Explicitly inserting BRNH nodes for fanouts...\n";
    analyzer.insertBranchNodes();
    
    std::cout << "3. Levelizing graph (Topological Sort)...\n";
    analyzer.levelizeGraph();
    
    // --- START SCOAP TIMER ---
    std::cout << "4. Running Deterministic Analysis (SCOAP)...\n";
    auto start_scoap = std::chrono::high_resolution_clock::now();
    
    analyzer.calculateSCOAP();
    
    auto end_scoap = std::chrono::high_resolution_clock::now();
    auto scoap_time = std::chrono::duration_cast<std::chrono::microseconds>(end_scoap - start_scoap).count();
    // --- END SCOAP TIMER ---

    // --- START COP TIMER ---
    std::cout << "5. Running Random Pattern Analysis (COP)...\n";
    auto start_cop = std::chrono::high_resolution_clock::now();
    
    analyzer.calculateCOP();
    
    auto end_cop = std::chrono::high_resolution_clock::now();
    auto cop_time = std::chrono::duration_cast<std::chrono::microseconds>(end_cop - start_cop).count();
    // --- END COP TIMER ---
    
    analyzer.printResults();
    analyzer.generateBottleneckReport();
    analyzer.exportGraphviz("circuit_graph.dot");

    // NEW: Print the Complexity Metrics
    std::cout << "\n================= COMPUTATIONAL COMPLEXITY METRICS =================\n";
    std::cout << "Theoretical Complexity : O(V + E) [Linear]\n";
    std::cout << "Total Nodes (V)        : " << analyzer.graph.size() << " (Gates + Branches + I/O)\n";
    std::cout << "SCOAP Execution Time   : " << scoap_time << " microseconds\n";
    std::cout << "COP Execution Time     : " << cop_time << " microseconds\n";
    std::cout << "====================================================================\n";

    return 0;
}