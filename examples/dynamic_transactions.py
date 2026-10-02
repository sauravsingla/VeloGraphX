"""Apply a graph update while maintaining incremental BFS state."""

import velographx as vx


graph = vx.Graph(4, False)
initial = vx.UpdateBatch()
initial.add(0, 1)
initial.add(1, 2)
initial.add(2, 3)
graph.apply(initial)

bfs = vx.IncrementalBFS(graph, 0)
print("before:", bfs.distances)

update = vx.UpdateBatch()
update.add(0, 3)

# Applying through the maintained algorithm updates both the graph and its BFS
# state, so callers do not need to rebuild either object after every batch.
bfs.apply(update)
print("after: ", bfs.distances)
