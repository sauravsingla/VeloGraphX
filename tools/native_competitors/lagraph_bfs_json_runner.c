#include <LAGraph.h>
#include <GraphBLAS.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *msg) { fprintf(stderr, "%s\n", msg); exit(2); }

int main(int argc, char **argv) {
    const char *dataset = NULL;
    int64_t source = -1, vertices = -1;
    bool directed = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dataset") && i + 1 < argc) dataset = argv[++i];
        else if (!strcmp(argv[i], "--source") && i + 1 < argc) source = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--vertices") && i + 1 < argc) vertices = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--directed")) directed = true;
        else die("invalid argument");
    }
    if (!dataset || source < 0 || vertices <= 0 || source >= vertices) die("invalid runner arguments");

    FILE *f = fopen(dataset, "r");
    if (!f) die("cannot open dataset");

    char msg[LAGRAPH_MSG_LEN];
    if (LAGraph_Init(msg) < 0) die(msg);
    GrB_Matrix A = NULL;
    if (GrB_Matrix_new(&A, GrB_BOOL, (GrB_Index) vertices, (GrB_Index) vertices) < 0) die("GrB_Matrix_new failed");

    long long u, v;
    while (fscanf(f, "%lld %lld", &u, &v) == 2) {
        if (u < 0 || v < 0 || u >= vertices || v >= vertices) die("edge vertex out of range");
        if (GrB_Matrix_setElement_BOOL(A, true, (GrB_Index) u, (GrB_Index) v) < 0) die("set edge failed");
        if (!directed && u != v) {
            if (GrB_Matrix_setElement_BOOL(A, true, (GrB_Index) v, (GrB_Index) u) < 0) die("set reverse edge failed");
        }
    }
    fclose(f);

    LAGraph_Graph G = NULL;
    LAGraph_Kind kind = directed ? LAGraph_ADJACENCY_DIRECTED : LAGraph_ADJACENCY_UNDIRECTED;
    if (LAGraph_New(&G, &A, kind, msg) < 0) die(msg);
    int cache_status = LAGraph_Cached_OutDegree(G, msg);
    if (cache_status < 0) die(msg);

    GrB_Vector level = NULL;
    if (LAGr_BreadthFirstSearch(&level, NULL, G, source, msg) < 0) die(msg);

    printf("{\"framework_version\":\"LAGraph-1.2.2\",\"distances\":[");
    for (int64_t i = 0; i < vertices; i++) {
        int64_t d = -1;
        GrB_Info info = GrB_Vector_extractElement_INT64(&d, level, (GrB_Index) i);
        if (info == GrB_NO_VALUE) d = -1;
        else if (info < 0) die("extract level failed");
        if (i) putchar(',');
        printf("%lld", (long long) d);
    }
    printf("]}\n");

    GrB_free(&level);
    LAGraph_Delete(&G, msg);
    LAGraph_Finalize(msg);
    return 0;
}
