#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace std;

enum GateType { INPUT, OUTPUT, AND, NAND, OR, NOR, XOR, XNOR, NOT, BRNH, BUF, UNKNOWN };

struct Node {
    int id;
    string name;
    GateType type;
    bool isOutput = false;

    vector<int> inputs;
    vector<int> outputs;

    int level = -1;

    int CC0 = 999999;
    int CC1 = 999999;
    int CO  = 999999;

    double prob1 = 0.0;
    double obs   = 0.0;
};

class CircuitAnalyzer {
public:
    vector<Node> graph;
    map<string, int> nameToId;
    int nextId = 0;
    vector<int> sortedNodes;

    int getOrCreateNode(const string& name) {
        if (nameToId.find(name) == nameToId.end()) {
            Node n;
            n.id   = nextId;
            n.name = name;
            n.type = UNKNOWN;
            graph.push_back(n);
            nameToId[name] = nextId++;
        }
        return nameToId[name];
    }

    GateType stringToGateType(const string& str) {
        if (str == "AND"  || str == "and")  return AND;
        if (str == "NAND" || str == "nand") return NAND;
        if (str == "OR"   || str == "or")   return OR;
        if (str == "NOR"  || str == "nor")  return NOR;
        if (str == "XOR"  || str == "xor")  return XOR;
        if (str == "XNOR" || str == "xnor") return XNOR;
        if (str == "NOT"  || str == "not")  return NOT;
        if (str == "BUF"  || str == "buf")  return BUF;
        return UNKNOWN;
    }

    void parseVerilog(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open file " << filename << endl;
            return;
        }

        string line;
        while (getline(file, line)) {
            // Strip inline comments
            size_t commentPos = line.find("//");
            if (commentPos != string::npos) line = line.substr(0, commentPos);

            string cleanLine = line;
            replace(cleanLine.begin(), cleanLine.end(), ',', ' ');
            replace(cleanLine.begin(), cleanLine.end(), ';', ' ');

            stringstream ss(cleanLine);
            string kw;
            if (!(ss >> kw)) continue;

            if (kw == "module" || kw == "endmodule" || kw == "wire") continue;

            if (kw == "input" || kw == "output") {
                GateType t = (kw == "input") ? INPUT : OUTPUT;
                string tok;
                bool hasRange = false;
                int low = 0, high = 0;

                while (ss >> tok) {
                    if (tok[0] == '[') {
                        size_t colon = tok.find(':');
                        size_t rbrac = tok.find(']');
                        if (colon != string::npos && rbrac != string::npos) {
                            int v1 = stoi(tok.substr(1, colon - 1));
                            int v2 = stoi(tok.substr(colon + 1, rbrac - colon - 1));
                            low  = min(v1, v2);
                            high = max(v1, v2);
                            hasRange = true;
                        }
                    } else {
                        if (hasRange) {
                            for (int bit = low; bit <= high; bit++) {
                                string bitName = tok + "[" + to_string(bit) + "]";
                                int id = getOrCreateNode(bitName);
                                if (graph[id].type == UNKNOWN) graph[id].type = t;
                                
                                if (t == OUTPUT) graph[id].isOutput = true;
                            }
                        } else {
                            int id = getOrCreateNode(tok);
                            if (graph[id].type == UNKNOWN) graph[id].type = t;
                            if (t == OUTPUT) graph[id].isOutput = true;
                        }
                    }
                }
                continue;
            }

            GateType t = stringToGateType(kw);
            if (t != UNKNOWN) {
                size_t startParen = line.find('(');
                size_t endParen   = line.find(')');
                if (startParen != string::npos && endParen != string::npos) {
                    string portsStr = line.substr(startParen + 1, endParen - startParen - 1);
                    replace(portsStr.begin(), portsStr.end(), ',', ' ');
                    stringstream portSS(portsStr);
                    string portTok;
                    vector<string> ports;
                    while (portSS >> portTok) ports.push_back(portTok);

                    if (!ports.empty()) {
                        int outId = getOrCreateNode(ports[0]);

                        graph[outId].type = t;

                        for (size_t i = 1; i < ports.size(); i++) {
                            string tok = ports[i];
                            int inId;
                            if (!tok.empty() && tok[0] == '~') {
                                // Inline inversion: insert an implicit NOT node
                                string baseName = tok.substr(1);
                                int baseId = getOrCreateNode(baseName);
                                string notName = "__not_" + baseName;
                                inId = getOrCreateNode(notName);
                                if (graph[inId].type == UNKNOWN) {
                                    graph[inId].type = NOT;
                                    graph[inId].inputs.push_back(baseId);
                                    graph[baseId].outputs.push_back(inId);
                                }
                            } else {
                                inId = getOrCreateNode(tok);
                            }
                            graph[outId].inputs.push_back(inId);
                            graph[inId].outputs.push_back(outId);
                        }
                    }
                }
            }
        }
    }

    void insertBranchNodes() {
        // Reserve space to avoid reallocation while pushing branch nodes,
        // which would invalidate any internal pointers/iterators to graph elements.
        graph.reserve(graph.size() * 3);

        int originalSize = (int)graph.size();
        for (int i = 0; i < originalSize; i++) {
            if (graph[i].outputs.size() > 1 && !graph[i].isOutput) {
                vector<int> originalOutputs = graph[i].outputs;
                graph[i].outputs.clear();
                for (size_t j = 0; j < originalOutputs.size(); j++) {
                    int targetId = originalOutputs[j];
                    string branchName = graph[i].name + "_brnh" + to_string(j + 1);
                    int branchId = getOrCreateNode(branchName);
                    graph[branchId].type = BRNH;
                    graph[i].outputs.push_back(branchId);
                    graph[branchId].inputs.push_back(i);
                    graph[branchId].outputs.push_back(targetId);
                    replace(graph[targetId].inputs.begin(),
                            graph[targetId].inputs.end(), i, branchId);
                }
            }
        }
    }

    void levelizeGraph() {
        sortedNodes.clear();
        vector<int> in_degree((int)graph.size(), 0);
        queue<int> q;

        for (const auto& node : graph)
            for (int out_id : node.outputs) in_degree[out_id]++;

        for (size_t i = 0; i < graph.size(); ++i) {
            if (in_degree[i] == 0) {
                q.push((int)i);
                graph[i].level = 0;
            }
        }

        while (!q.empty()) {
            int u = q.front(); q.pop();
            sortedNodes.push_back(u);
            for (int v_id : graph[u].outputs) {
                if (--in_degree[v_id] == 0) {
                    q.push(v_id);
                    graph[v_id].level = graph[u].level + 1;
                }
            }
        }

        // Detect combinational loops — nodes stuck in a cycle never reach in_degree=0
        // and are silently excluded from sortedNodes without this check.
        if (sortedNodes.size() != graph.size()) {
            cerr << "[WARNING] Circuit contains a combinational loop or disconnected nodes! "
                 << (graph.size() - sortedNodes.size())
                 << " node(s) unreachable and excluded from analysis.\n";
        }
    }

    void calculateSCOAP() {
        // Forward pass: compute CC0 and CC1 
        for (int id : sortedNodes) {
            Node& n = graph[id];

            if (n.type == INPUT) {
                n.CC0 = 1; n.CC1 = 1;
                continue;
            }
            if (n.inputs.empty()) continue;

            if (n.type == AND) {
                n.CC1 = 1; n.CC0 = 999999;
                for (int inId : n.inputs) {
                    n.CC1 += graph[inId].CC1;
                    n.CC0  = min(n.CC0, graph[inId].CC0);
                }
                n.CC0 += 1;

            } else if (n.type == NAND) {
                n.CC0 = 1; n.CC1 = 999999;
                for (int inId : n.inputs) {
                    n.CC0 += graph[inId].CC1;
                    n.CC1  = min(n.CC1, graph[inId].CC0);
                }
                n.CC1 += 1;

            } else if (n.type == OR) {
                n.CC0 = 1; n.CC1 = 999999;
                for (int inId : n.inputs) {
                    n.CC0 += graph[inId].CC0;
                    n.CC1  = min(n.CC1, graph[inId].CC1);
                }
                n.CC1 += 1;

            } else if (n.type == NOR) {
                n.CC1 = 1; n.CC0 = 999999;
                for (int inId : n.inputs) {
                    n.CC1 += graph[inId].CC0;
                    n.CC0  = min(n.CC0, graph[inId].CC1);
                }
                n.CC0 += 1;

            } else if (n.type == NOT) {
                n.CC1 = graph[n.inputs[0]].CC0 + 1;
                n.CC0 = graph[n.inputs[0]].CC1 + 1;

            } else if (n.type == XOR || n.type == XNOR) {
                if (n.inputs.size() >= 2) {
                    int cc0 = graph[n.inputs[0]].CC0;
                    int cc1 = graph[n.inputs[0]].CC1;
                    for (size_t i = 1; i < n.inputs.size(); ++i) {
                        int n_cc0 = graph[n.inputs[i]].CC0;
                        int n_cc1 = graph[n.inputs[i]].CC1;
                        int new_cc0 = min(cc0 + n_cc0, cc1 + n_cc1);
                        int new_cc1 = min(cc1 + n_cc0, cc0 + n_cc1);
                        cc0 = new_cc0;
                        cc1 = new_cc1;
                    }
                    n.CC0 = cc0 + 1;
                    n.CC1 = cc1 + 1;
                    if (n.type == XNOR) swap(n.CC0, n.CC1);
                }

            } else if (n.type == BRNH || n.type == OUTPUT || n.type == BUF) {
                // Wire/buffer: pass controllability straight through
                n.CC0 = graph[n.inputs[0]].CC0;
                n.CC1 = graph[n.inputs[0]].CC1;
            }
        }

        // Backward pass: compute CO
        // Primary outputs are always observable at zero cost
        for (int id : sortedNodes)
            if (graph[id].isOutput) graph[id].CO = 0;

        for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
            Node& n = graph[*it];

            if (n.inputs.empty() || n.CO > 900000) continue;

            // BRNH passes observability with NO added cost (+1).
            // Handle it separately and skip the gate-input loop below.
            if (n.type == BRNH) {
                graph[n.inputs[0]].CO = min(graph[n.inputs[0]].CO, n.CO);
                continue;
            }

            for (size_t i = 0; i < n.inputs.size(); i++) {
                int& inCO = graph[n.inputs[i]].CO;
                int side = 0;

                if (n.type == AND || n.type == NAND) {
                    for (size_t j = 0; j < n.inputs.size(); j++)
                        if (i != j) side += graph[n.inputs[j]].CC1;

                } else if (n.type == OR || n.type == NOR) {
                    
                    for (size_t j = 0; j < n.inputs.size(); j++)
                        if (i != j) side += graph[n.inputs[j]].CC0;

                } else if (n.type == XOR || n.type == XNOR) {
                    for (size_t j = 0; j < n.inputs.size(); j++)
                        if (i != j) side += min(graph[n.inputs[j]].CC0, graph[n.inputs[j]].CC1);

                }

                inCO = min(inCO, n.CO + side + 1);
            }
        }
    }

    void calculateCOP() {
        // Forward pass: compute signal probability prob1            
        for (int id : sortedNodes) {
            Node& n = graph[id];

            if (n.type == INPUT) { n.prob1 = 0.5; continue; }
            if (n.inputs.empty()) continue;

            if (n.type == AND) {
                n.prob1 = 1.0;
                for (int inId : n.inputs) n.prob1 *= graph[inId].prob1;

            } else if (n.type == OR) {
                double p0 = 1.0;
                for (int inId : n.inputs) p0 *= (1.0 - graph[inId].prob1);
                n.prob1 = 1.0 - p0;

            } else if (n.type == NAND) {
                double p = 1.0;
                for (int inId : n.inputs) p *= graph[inId].prob1;
                n.prob1 = 1.0 - p;

            } else if (n.type == NOR) {
                double p = 1.0;
                for (int inId : n.inputs) p *= (1.0 - graph[inId].prob1);
                n.prob1 = p;

            } else if (n.type == NOT) {
                n.prob1 = 1.0 - graph[n.inputs[0]].prob1;

            } else if (n.type == XOR || n.type == XNOR) {
                double p = graph[n.inputs[0]].prob1;
                for (size_t i = 1; i < n.inputs.size(); ++i) {
                    double pn = graph[n.inputs[i]].prob1;
                    p = p * (1.0 - pn) + (1.0 - p) * pn;
                }
                n.prob1 = (n.type == XNOR) ? 1.0 - p : p;

            } else if (n.type == BRNH || n.type == BUF || n.type == OUTPUT) {
                n.prob1 = graph[n.inputs[0]].prob1;
            }
        }

        // Backward pass: compute observability
        // Primary outputs are always fully observable
        for (int id : sortedNodes)
            if (graph[id].isOutput) graph[id].obs = 1.0;

        for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
            Node& n = graph[*it];

            if (n.inputs.empty() || n.obs <= 0.0) continue;
            if (n.type == BRNH) {
                double& fanInObs = graph[n.inputs[0]].obs;
                fanInObs = 1.0 - (1.0 - fanInObs) * (1.0 - n.obs);
                continue;
            }

            for (size_t i = 0; i < n.inputs.size(); i++) {
                double sens = 1.0;

                if (n.type == AND || n.type == NAND) {
                    // Sensitivity = probability all other inputs are 1
                    for (size_t j = 0; j < n.inputs.size(); j++)
                        if (i != j) sens *= graph[n.inputs[j]].prob1;

                } else if (n.type == OR || n.type == NOR) {
                    // Sensitivity = probability all other inputs are 0
                    for (size_t j = 0; j < n.inputs.size(); j++)
                        if (i != j) sens *= (1.0 - graph[n.inputs[j]].prob1);

                }
                // XOR / XNOR / NOT / BUF / OUTPUT: sens stays 1.0

                graph[n.inputs[i]].obs = max(graph[n.inputs[i]].obs, n.obs * sens);
            }
        }
    }

    void generateBottleneckReport() {
        cout << "\n=================================== BOTTLENECK REPORT ===================================\n";

        vector<int> internalNodes;
        for (int id : sortedNodes) {
            if (graph[id].type != INPUT && graph[id].type != BRNH)
                internalNodes.push_back(id);
        }

        if (internalNodes.empty()) {
            cout << "[INFO] No internal gate nodes found to analyze.\n";
            return;
        }

        // Top 5 hardest to control (highest combined CC0 + CC1)
        sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            return (graph[a].CC0 + graph[a].CC1) > (graph[b].CC0 + graph[b].CC1);
        });
        cout << "[Top 5 Hardest to Control (SCOAP)]\n";
        for (int i = 0; i < min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            cout << "  " << i+1 << ". " << n.name
                 << " (Combined CC=" << (n.CC0 + n.CC1)
                 << " | CC0=" << n.CC0 << ", CC1=" << n.CC1 << ")\n";
        }

        // Top 5 hardest to observe (highest CO, skip unobservable sentinels first)
        sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            return graph[a].CO > graph[b].CO;
        });
        cout << "\n[Top 5 Hardest to Observe (SCOAP CO)]\n";
        for (int i = 0; i < min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            cout << "  " << i+1 << ". " << n.name
                 << " (CO=" << (n.CO > 900000 ? -1 : n.CO) << ")\n";
        }

        // Top 5 random-pattern resistant (lowest detection probability)
        sort(internalNodes.begin(), internalNodes.end(), [&](int a, int b) {
            double dpA = min(graph[a].prob1 * graph[a].obs,
                            (1.0 - graph[a].prob1) * graph[a].obs);
            double dpB = min(graph[b].prob1 * graph[b].obs,
                            (1.0 - graph[b].prob1) * graph[b].obs);
            return dpA < dpB;
        });
        cout << "\n[Top 5 Random Pattern Resistant Nodes (COP DP)]\n";
        for (int i = 0; i < min(5, (int)internalNodes.size()); i++) {
            Node& n = graph[internalNodes[i]];
            double dp = min(n.prob1 * n.obs, (1.0 - n.prob1) * n.obs);
            double patterns = (dp > 1e-15)
                              ? (log(1.0 - 0.99) / log(1.0 - dp))
                              : 999999999.0;

            // Guard against long long overflow for extremely small dp values
            long long patternsNeeded = (patterns > 9e17)
                                       ? 999999999LL
                                       : (long long)patterns;

            cout << "  " << i+1 << ". " << n.name
                 << " (DetProb=" << scientific << setprecision(4) << dp
                 << " | Req. Patterns: " << patternsNeeded << ")\n";
        }
        cout << "=========================================================================================\n";
    }

    void printFinalReport() {
        cout << "\n" << string(115, '=') << "\n";
        cout << left
             << setw(15) << "Node Name"
             << setw(8)  << "CC0"
             << setw(8)  << "CC1"
             << setw(8)  << "CO"
             << setw(12) << "Prob(1)"
             << setw(12) << "Obs"
             << "DetProb(Min)" << "\n";
        cout << string(115, '-') << "\n";

        int    hSCOAP   = -1;
        double maxSCOAP = -1.0;
        int    hCOP     = -1;
        double minDP    = 2.0;

        for (int id : sortedNodes) {
            Node& n = graph[id];
            double dp = min(n.prob1 * n.obs, (1.0 - n.prob1) * n.obs);

            cout << left
                 << setw(15) << n.name
                 << setw(8)  << (n.CC0 > 9000 ? -1 : n.CC0)
                 << setw(8)  << (n.CC1 > 9000 ? -1 : n.CC1)
                 << setw(8)  << (n.CO  > 9000 ? -1 : n.CO)
                 << fixed << setprecision(4)
                 << setw(12) << n.prob1
                 << setw(12) << n.obs
                 << dp << "\n";

            // Exclude nodes with sentinel values from the worst-node ranking
            // to prevent unobservable nodes (CO=999999) from dominating the score.
            bool validForRanking = (n.type != INPUT && n.type != BRNH)
                                && (n.CC0 < 9000) && (n.CC1 < 9000) && (n.CO < 9000);

            if (validForRanking) {
                double sCost = (double)(n.CC0 + n.CC1 + n.CO);
                if (sCost > maxSCOAP) { maxSCOAP = sCost; hSCOAP = id; }
            }

            if (n.type != INPUT && n.type != BRNH && n.obs > 0.0 && dp < minDP) {
                minDP = dp; hCOP = id;
            }
        }

        cout << string(115, '=') << "\n";
        if (hSCOAP != -1)
            cout << ">>> HARD TO DETECT (SCOAP): " << graph[hSCOAP].name << "\n";
        if (hCOP != -1)
            cout << ">>> HARD TO DETECT (COP):   " << graph[hCOP].name
                 << " (Random Pattern Resistant)\n";
    }
};

#endif
