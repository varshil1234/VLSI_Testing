#include <iostream>
#include <chrono>
#include "Circuit.hpp"

using namespace std;
using namespace std::chrono;

int main() {
    CircuitAnalyzer analyzer;
    
    string filename = "circuit.v"; 
    
    cout << "1. Parsing structural Verilog from " << filename << "...\n";
    analyzer.parseVerilog(filename);
    
    cout << "2. Explicitly inserting BRNH nodes for fanouts...\n";
    analyzer.insertBranchNodes();
    
    cout << "3. Levelizing graph (Topological Sort)...\n";
    analyzer.levelizeGraph();
    
    cout << "4. Running Deterministic Analysis (SCOAP)...\n";
    auto start_scoap = high_resolution_clock::now();
    
    analyzer.calculateSCOAP();
    
    auto end_scoap = high_resolution_clock::now();
    auto scoap_time = duration_cast<microseconds>(end_scoap - start_scoap).count();

    cout << "5. Running Random Pattern Analysis (COP)...\n";
    auto start_cop = high_resolution_clock::now();
    
    analyzer.calculateCOP();
    
    auto end_cop = high_resolution_clock::now();
    auto cop_time = duration_cast<microseconds>(end_cop - start_cop).count();
    
    analyzer.printFinalReport();
    analyzer.generateBottleneckReport();

    cout << "\n================= COMPUTATIONAL COMPLEXITY METRICS =================\n";
    cout << "Theoretical Complexity : O(V + E) [Linear]\n";
    cout << "Total Nodes (V)        : " << analyzer.graph.size() << " (Gates + Branches + I/O)\n";
    cout << "SCOAP Execution Time   : " << scoap_time << " microseconds\n";
    cout << "COP Execution Time     : " << cop_time << " microseconds\n";
    cout << "====================================================================\n";

    return 0;
}
