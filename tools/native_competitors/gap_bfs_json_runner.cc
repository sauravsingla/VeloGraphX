#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Compile the pinned GAP Benchmark Suite BFS implementation in this same
// translation unit. This keeps the runner on GAPBS v1.5's native C++11 build
// model and avoids a fragile cross-translation-unit re-declaration boundary.
// Rename only GAP's CLI entry point; DOBFS and BFSVerifier remain unchanged.
#define main gapbs_original_main
#include "bfs.cc"
#undef main

static void die(const std::string &msg) {
    std::cerr << msg << "\n";
    std::exit(2);
}

int main(int argc, char **argv) {
    std::string dataset;
    int source = -1;
    int vertices = -1;
    bool directed = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dataset" && i + 1 < argc) dataset = argv[++i];
        else if (a == "--source" && i + 1 < argc) source = std::stoi(argv[++i]);
        else if (a == "--vertices" && i + 1 < argc) vertices = std::stoi(argv[++i]);
        else if (a == "--directed") directed = true;
        else die("invalid argument");
    }
    if (dataset.empty() || source < 0 || vertices <= 0 || source >= vertices) {
        die("invalid runner arguments");
    }

    std::vector<std::string> args = {"gap-json-runner"};
    if (!directed) args.push_back("-s");
    args.push_back("-f");
    args.push_back(dataset);
    args.push_back("-r");
    args.push_back(std::to_string(source));
    std::vector<char *> cargs;
    cargs.reserve(args.size());
    for (auto &s : args) cargs.push_back(&s[0]);

    CLApp cli(static_cast<int>(cargs.size()), cargs.data(),
              "normalized breadth-first search");
    if (!cli.ParseArgs()) die("GAP argument parsing failed");
    Builder b(cli);
    Graph g = b.MakeGraph();
    if (g.num_nodes() != vertices) die("GAP graph vertex count mismatch");

    pvector<NodeID> parent = DOBFS(g, static_cast<NodeID>(source), false, 15, 18);
    if (!BFSVerifier(g, static_cast<NodeID>(source), parent)) {
        die("GAP BFS verification failed");
    }

    std::vector<int> dist(vertices, -1);
    dist[source] = 0;
    for (int v = 0; v < vertices; ++v) {
        if (v == source || parent[v] < 0) continue;
        NodeID cur = static_cast<NodeID>(v);
        int depth = 0;
        while (cur != source) {
            if (cur < 0 || cur >= vertices || parent[cur] < 0) {
                depth = -1;
                break;
            }
            cur = parent[cur];
            ++depth;
            if (depth > vertices) die("cycle detected in GAP BFS parent tree");
        }
        if (depth >= 0) dist[v] = depth;
    }

    std::cout << "{\"framework_version\":\"GAPBS-1.5\",\"distances\":[";
    for (int i = 0; i < vertices; ++i) {
        if (i) std::cout << ',';
        std::cout << dist[i];
    }
    std::cout << "]}\n";
    return 0;
}
