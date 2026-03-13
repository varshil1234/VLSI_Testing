#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath> // NEW: Required for the log() function in the Bottleneck report

enum GateType { INPUT, OUTPUT, AND, NAND, OR, NOR, XOR, XNOR, NOT, BRNH, UNKNOWN };

struct Node {
    int id;                   
    std::string name;         
    GateType type; 
    bool isOutput = false;    
    
    std::vector<int> inputs;  
    std::vector<int> outputs; 
    
    int level = -1;

    // SCOAP Variables
    int CC0 = 999999;
    int CC1 = 999999;
    int CO = 999999;

    // COP Variables
    double prob1 = 0.0;
    double obs = 0.0;
};

class CircuitAnalyzer {
public:
    std::vector<Node> graph;
    std::map<std::string, int> nameToId;
    int nextId = 0;
    std::vector<int> sortedNodes; 

    int getOrCreateNode(const std::string& name) {
        if (nameToId.find(name) == nameToId.end()) {
            Node n;
            n.id = nextId;
            n.name = name;
            n.type = UNKNOWN;
            graph.push_back(n);
            nameToId[name] = nextId++;
        }
        return nameToId[name];
    }

    GateType stringToGateType(const std::string& str) {
        if (str == "AND" || str == "and") return AND;
        if (str == "NAND" || str == "nand") return NAND;
        if (str == "OR" || str == "or") return OR;
        if (str == "NOR" || str == "nor") return NOR;
        if (str == "XOR" || str == "xor") return XOR;
        if (str == "XNOR" || str == "xnor") return XNOR;
        if (str == "NOT" || str == "not") return NOT;
        return UNKNOWN;
    }

    // --- PARSER ---
    void parseVerilog(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;

        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return;
        }

        while (std::getline(file, line)) {
            size_t commentPos = line.find("//");
            if (commentPos != std::string::npos) line = line.substr(0, commentPos);
            
            for (char& c : line) {
                if (c == '(' || c == ')' || c == ',' || c == ';') c = ' ';
            }

            std::stringstream ss(line);
            std::string word;
            std::vector<std::string> tokens;
            while (ss >> word) tokens.push_back(word);

            if (tokens.empty() || tokens[0] == "module" || tokens[0] == "endmodule" || tokens[0] == "wire") {
                continue; 
            }

            if (tokens[0] == "input" || tokens[0] == "output") {
                GateType t = (tokens[0] == "input") ? INPUT : OUTPUT;
                for (size_t i = 1; i < tokens.size(); i++) {
                    int id = getOrCreateNode(tokens[i]);
                    graph[id].type = t;
                    if (t == OUTPUT) graph[id].isOutput = true; 
                }
            } else {
                GateType t = stringToGateType(tokens[0]);
                if (t != UNKNOWN && tokens.size() >= 3) {
                    int outId = getOrCreateNode(tokens[1]);
                    graph[outId].type = t; 
                    
                    for (size_t i = 2; i < tokens.size(); i++) {
                        int inId = getOrCreateNode(tokens[i]);
                        graph[outId].inputs.push_back(inId);
                        graph[inId].outputs.push_back(outId);
                    }
                }
            }
        }
        file.close();
    }

    // --- ADD BRANCHES FOR FANOUT ---
    void insertBranchNodes() {
        int originalSize = graph.size();
        for (int i = 0; i < originalSize; i++) {
            if (graph[i].outputs.size() > 1 && !graph[i].isOutput) {
                std::vector<int> originalOutputs = graph[i].outputs;
                graph[i].outputs.clear();

                for (size_t j = 0; j < originalOutputs.size(); j++) {
                    int targetId = originalOutputs[j];
                    std::string branchName = graph[i].name + "_" + std::to_string(j+1);
                    int branchId = getOrCreateNode(branchName);
                    
                    graph[branchId].type = BRNH;
                    graph[i].outputs.push_back(branchId);
                    graph[branchId].inputs.push_back(i);
                    graph[branchId].outputs.push_back(targetId);
                    
                    std::replace(graph[targetId].inputs.begin(), graph[targetId].inputs.end(), i, branchId);
                }
            }
        }
    }

    // --- LEVELIZATION ---
    void levelizeGraph() {
        std::queue<int> q;
        for (auto& node : graph) {
            if (node.type == INPUT) {
                node.level = 0;
                q.push(node.id);
            }
        }

        while (!q.empty()) {
            int current = q.front();
            q.pop();

            for (int outId : graph[current].outputs) {
                if (graph[outId].level < graph[current].level + 1) {
                    graph[outId].level = graph[current].level + 1;
                    q.push(outId);
                }
            }
        }

        for (int i = 0; i < graph.size(); i++) sortedNodes.push_back(i);
        std::sort(sortedNodes.begin(), sortedNodes.end(), [&](int a, int b) {
            return graph[a].level < graph[b].level;
        });
    }

    // --- SCOAP ANALYSIS ---
    void calculateSCOAP() {
        for (int id : sortedNodes) {
            Node& n = graph[id];
            if (n.type == INPUT) { n.CC0 = 1; n.CC1 = 1; continue; }
            if (n.inputs.empty()) continue;

            if (n.type == AND) {
                n.CC1 = 1; n.CC0 = 999999;
                for (int inId : n.inputs) { n.CC1 += graph[inId].CC1; n.CC0 = std::min(n.CC0, graph[inId].CC0); }
                n.CC0 += 1;
            } else if (n.type == NAND) {
                n.CC0 = 1; n.CC1 = 999999;
                for (int inId : n.inputs) { n.CC0 += graph[inId].CC1; n.CC1 = std::min(n.CC1, graph[inId].CC0); }
                n.CC1 += 1;
            } else if (n.type == OR) {
                n.CC0 = 1; n.CC1 = 999999;
                for (int inId : n.inputs) { n.CC0 += graph[inId].CC0; n.CC1 = std::min(n.CC1, graph[inId].CC1); }
                n.CC1 += 1;
            } else if (n.type == NOR) {
                n.CC1 = 1; n.CC0 = 999999;
                for (int inId : n.inputs) { n.CC1 += graph[inId].CC0; n.CC0 = std::min(n.CC0, graph[inId].CC1); }
                n.CC0 += 1;
            } else if (n.type == NOT) {
                n.CC1 = graph[n.inputs[0]].CC0 + 1; n.CC0 = graph[n.inputs[0]].CC1 + 1;
            } else if (n.type == XOR) {
                int a = n.inputs[0], b = n.inputs[1];
                n.CC1 = std::min(graph[a].CC1 + graph[b].CC0, graph[a].CC0 + graph[b].CC1) + 1;
                n.CC0 = std::min(graph[a].CC0 + graph[b].CC0, graph[a].CC1 + graph[b].CC1) + 1;
            } else if (n.type == XNOR) {
                int a = n.inputs[0], b = n.inputs[1];
                n.CC1 = std::min(graph[a].CC1 + graph[b].CC1, graph[a].CC0 + graph[b].CC0) + 1;
                n.CC0 = std::min(graph[a].CC1 + graph[b].CC0, graph[a].CC0 + graph[b].CC1) + 1;
            } else if (n.type == BRNH || n.type == OUTPUT) {
                n.CC1 = graph[n.inputs[0]].CC1; n.CC0 = graph[n.inputs[0]].CC0;
            }
        }

        for (int id : sortedNodes) if (graph[id].isOutput) graph[id].CO = 0;

        for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
            Node& n = graph[*it];
            if (n.inputs.empty()) continue; 

            if (n.type == AND || n.type == NAND) {
                for (size_t i = 0; i < n.inputs.size(); i++) {
                    int sideCost = 0;
                    for (size_t j = 0; j < n.inputs.size(); j++) if (i != j) sideCost += graph[n.inputs[j]].CC1;
                    graph[n.inputs[i]].CO = std::min(graph[n.inputs[i]].CO, n.CO + sideCost + 1);
                }
            } else if (n.type == OR || n.type == NOR) {
                for (size_t i = 0; i < n.inputs.size(); i++) {
                    int sideCost = 0;
                    for (size_t j = 0; j < n.inputs.size(); j++) if (i != j) sideCost += graph[n.inputs[j]].CC0;
                    graph[n.inputs[i]].CO = std::min(graph[n.inputs[i]].CO, n.CO + sideCost + 1);
                }
            } else if (n.type == XOR || n.type == XNOR) {
                int a = n.inputs[0], b = n.inputs[1];
                graph[a].CO = std::min(graph[a].CO, n.CO + std::min(graph[b].CC0, graph[b].CC1) + 1);
                graph[b].CO = std::min(graph[b].CO, n.CO + std::min(graph[a].CC0, graph[a].CC1) + 1);
            } else if (n.type == NOT || n.type == OUTPUT) {
                graph[n.inputs[0]].CO = std::min(graph[n.inputs[0]].CO, n.CO + 1);
            } else if (n.type == BRNH) {
                graph[n.inputs[0]].CO = std::min(graph[n.inputs[0]].CO, n.CO);
            }
        }
    }

    // --- COP ANALYSIS ---
    void calculateCOP() {
        for (int id : sortedNodes) {
            Node& n = graph[id];
            if (n.type == INPUT) { n.prob1 = 0.5; continue; }
            if (n.inputs.empty()) continue;

            if (n.type == AND) {
                n.prob1 = 1.0; for (int inId : n.inputs) n.prob1 *= graph[inId].prob1;
            } else if (n.type == NAND) {
                double p1 = 1.0; for (int inId : n.inputs) p1 *= graph[inId].prob1; n.prob1 = 1.0 - p1;
            } else if (n.type == OR) {
                double p0 = 1.0; for (int inId : n.inputs) p0 *= (1.0 - graph[inId].prob1); n.prob1 = 1.0 - p0;
            } else if (n.type == NOR) {
                double p0 = 1.0; for (int inId : n.inputs) p0 *= (1.0 - graph[inId].prob1); n.prob1 = p0;
            } else if (n.type == NOT) {
                n.prob1 = 1.0 - graph[n.inputs[0]].prob1;
            } else if (n.type == XOR) {
                double pA = graph[n.inputs[0]].prob1, pB = graph[n.inputs[1]].prob1;
                n.prob1 = (pA * (1.0 - pB)) + (pB * (1.0 - pA));
            } else if (n.type == XNOR) {
                double pA = graph[n.inputs[0]].prob1, pB = graph[n.inputs[1]].prob1;
                n.prob1 = (pA * pB) + ((1.0 - pA) * (1.0 - pB));
            } else if (n.type == BRNH || n.type == OUTPUT) {
                n.prob1 = graph[n.inputs[0]].prob1;
            }
        }

        for (int id : sortedNodes) if (graph[id].isOutput) graph[id].obs = 1.0;

        for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
            Node& n = graph[*it];
            if (n.inputs.empty()) continue; 

            if (n.type == AND || n.type == NAND) {
                for (size_t i = 0; i < n.inputs.size(); i++) {
                    double sideProb = 1.0;
                    for (size_t j = 0; j < n.inputs.size(); j++) if (i != j) sideProb *= graph[n.inputs[j]].prob1;
                    graph[n.inputs[i]].obs = std::max(graph[n.inputs[i]].obs, n.obs * sideProb);
                }
            } else if (n.type == OR || n.type == NOR) {
                for (size_t i = 0; i < n.inputs.size(); i++) {
                    double sideProb = 1.0;
                    for (size_t j = 0; j < n.inputs.size(); j++) if (i != j) sideProb *= (1.0 - graph[n.inputs[j]].prob1);
                    graph[n.inputs[i]].obs = std::max(graph[n.inputs[i]].obs, n.obs * sideProb);
                }
            } else if (n.type == XOR || n.type == XNOR) {
                graph[n.inputs[0]].obs = std::max(graph[n.inputs[0]].obs, n.obs);
                graph[n.inputs[1]].obs = std::max(graph[n.inputs[1]].obs, n.obs);
            } else if (n.type == NOT || n.type == OUTPUT) {
                graph[n.inputs[0]].obs = std::max(graph[n.inputs[0]].obs, n.obs);
            } else if (n.type == BRNH) {
                 graph[n.inputs[0]].obs = std::max(graph[n.inputs[0]].obs, n.obs); 
            }
        }
    }

    void printResults() {
        std::cout << "\n=================================== TESTABILITY ANALYSIS REPORT ===================================\n";
        std::cout << std::left << std::setw(15) << "Node Name" 
                  << std::setw(12) << "Type"
                  << std::setw(10) << "Level"
                  << std::setw(10) << "CC0" 
                  << std::setw(10) << "CC1" 
                  << std::setw(10) << "CO" 
                  << std::setw(15) << "COP(P=1)" 
                  << "COP(Obs)" << "\n";
        std::cout << std::string(95, '-') << "\n";
        
        std::string typeNames[] = {"INPUT", "OUTPUT", "AND", "NAND", "OR", "NOR", "XOR", "XNOR", "NOT", "BRNH", "UNKNOWN"};

        for (int id : sortedNodes) {
            const Node& n = graph[id];
            
            std::string displayType = typeNames[n.type];
            if (n.isOutput && n.type != OUTPUT) displayType += " (PO)";

            std::cout << std::left << std::setw(15) << n.name 
                      << std::setw(12) << displayType
                      << std::setw(10) << n.level
                      << std::setw(10) << (n.CC0 > 9000 ? -1 : n.CC0) 
                      << std::setw(10) << (n.CC1 > 9000 ? -1 : n.CC1) 
                      << std::setw(10) << (n.CO > 9000 ? -1 : n.CO) 
                      << std::setw(15) << std::fixed << std::setprecision(6) << n.prob1 
                      << std::fixed << std::setprecision(6) << n.obs << "\n";
        }
    }

    // --- FEATURE 1: BOTTLENECK REPORT ---
    void generateBottleneckReport() {
        std::cout << "\n=================================== BOTTLENECK REPORT ===================================\n";
        
        // Filter out primary inputs/outputs to focus on internal logic bottlenecks
        std::vector<int> internalNodes;
        for (int id : sortedNodes) {
            if (graph[id].type != INPUT && !graph[id].isOutput && graph[id].type != BRNH) {
                internalNodes.push_back(id);
            }
        }

        if (internalNodes.empty()) return;

        // 1. Hardest to Control (Highest CC0 + CC1)
        std::sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            return (graph[a].CC0 + graph[a].CC1) > (graph[b].CC0 + graph[b].CC1);
        });
        std::cout << "[Top 5 Hardest to Control (SCOAP)]\n";
        for (int i = 0; i < std::min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            std::cout << "  " << i+1 << ". " << n.name << " (CC0: " << n.CC0 << ", CC1: " << n.CC1 << ")\n";
        }

        // 2. Hardest to Observe (Highest CO)
        std::sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            return graph[a].CO > graph[b].CO;
        });
        std::cout << "\n[Top 5 Hardest to Observe (SCOAP)]\n";
        for (int i = 0; i < std::min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            std::cout << "  " << i+1 << ". " << n.name << " (CO: " << n.CO << ")\n";
        }

        // 3. Random Pattern Resistant (RPR) Faults (Lowest COP Detection Prob)
        std::sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            double dpA = std::min(graph[a].prob1 * graph[a].obs, (1.0 - graph[a].prob1) * graph[a].obs);
            double dpB = std::min(graph[b].prob1 * graph[b].obs, (1.0 - graph[b].prob1) * graph[b].obs);
            return dpA < dpB; // Ascending order
        });
        std::cout << "\n[Top 5 Random Pattern Resistant Nodes (COP)]\n";
        for (int i = 0; i < std::min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            double dp = std::min(n.prob1 * n.obs, (1.0 - n.prob1) * n.obs);
            
            // Calculate Required Patterns for 99% confidence: N = ln(1 - 0.99) / ln(1 - DP)
            double requiredPatterns = (dp > 0) ? (log(1.0 - 0.99) / log(1.0 - dp)) : 999999;
            
            std::cout << "  " << i+1 << ". " << n.name << " (Detection Prob: " 
                      << std::fixed << std::setprecision(6) << dp 
                      << " | Est. Patterns (99%): " << (int)requiredPatterns << ")\n";
        }
        std::cout << "=========================================================================================\n";
    }

    // --- FEATURE 2: GRAPHVIZ EXPORTER ---
    // --- FEATURE 2: GRAPHVIZ EXPORTER (UPGRADED) ---
    void exportGraphviz(const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: Could not create " << filename << "\n";
            return;
        }

        out << "digraph CircuitGraph {\n";
        out << "  rankdir=LR;\n"; // Left-to-Right layout
        out << "  nodesep=0.8;\n"; // Add horizontal spacing between nodes
        out << "  ranksep=1.2;\n"; // Add vertical spacing between levels
        out << "  node [shape=plaintext, fontname=\"Arial, sans-serif\"];\n"; // plaintext required for HTML tables
        out << "  edge [color=\"#555555\", penwidth=1.5, arrowsize=0.8];\n\n";

        std::string typeNames[] = {"INPUT", "OUTPUT", "AND", "NAND", "OR", "NOR", "XOR", "XNOR", "NOT", "BRNH", "UNKNOWN"};

        // 1. Define the nodes with HTML Table Labels
        for (const Node& n : graph) {
            std::string displayType = typeNames[n.type];
            if (n.isOutput && n.type != OUTPUT) displayType += " (PO)";

            // Color Coding
            std::string headerBg = "#34495e"; // Default Dark Blue (Internal Gates)
            if (n.type == INPUT) { headerBg = "#27ae60"; } // Green for Inputs
            else if (n.isOutput) { headerBg = "#c0392b"; } // Red for Outputs
            else if (n.type == BRNH) { headerBg = "#e67e22"; } // Orange for Branches

            // Create an HTML table for the node label
            std::ostringstream label;
            label << "<\n<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" COLOR=\"#aaaaaa\">\n"
                  << "  <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << headerBg << "\"><FONT COLOR=\"white\"><B>" << n.name << "</B>  [" << displayType << "]</FONT></TD></TR>\n"
                  << "  <TR><TD ALIGN=\"LEFT\" BGCOLOR=\"#f8f9fa\"><B>SCOAP</B> (CC0/CC1/CO)</TD><TD BGCOLOR=\"#f8f9fa\">" 
                  << (n.CC0 > 9000 ? -1 : n.CC0) << " / " << (n.CC1 > 9000 ? -1 : n.CC1) << " / " << (n.CO > 9000 ? -1 : n.CO) << "</TD></TR>\n"
                  << "  <TR><TD ALIGN=\"LEFT\" BGCOLOR=\"#ffffff\"><B>COP</B> (P=1)</TD><TD BGCOLOR=\"#ffffff\">" 
                  << std::fixed << std::setprecision(4) << n.prob1 << "</TD></TR>\n"
                  << "  <TR><TD ALIGN=\"LEFT\" BGCOLOR=\"#f8f9fa\"><B>COP</B> (Obs)</TD><TD BGCOLOR=\"#f8f9fa\">" 
                  << std::fixed << std::setprecision(4) << n.obs << "</TD></TR>\n"
                  << "</TABLE>\n>";

            out << "  node" << n.id << " [label=" << label.str() << "];\n";
        }

        out << "\n  // 2. Define Edges (Connections)\n";
        for (const Node& n : graph) {
            for (int outId : n.outputs) {
                out << "  node" << n.id << " -> node" << outId << ";\n";
            }
        }

        out << "\n  // 3. Structural Alignment (Force neatly aligned columns based on logic level)\n";
        int maxLevel = 0;
        for (const Node& n : graph) maxLevel = std::max(maxLevel, n.level);
        
        for (int l = 0; l <= maxLevel; l++) {
            out << "  { rank=same; ";
            for (const Node& n : graph) {
                if (n.level == l) out << "node" << n.id << "; ";
            }
            out << "}\n";
        }

        out << "}\n";
        out.close();
        std::cout << "\n[+] Visually Enhanced Schematic Graph successfully exported to: " << filename << "\n";
    }
};