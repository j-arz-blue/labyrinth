#include "graph_builder.h"

namespace labyrinth {

void GraphBuilder::addOutPath(OutPaths & out_paths, OutPath out_path) {
    out_paths.set(static_cast<size_t>(out_path));
}

void GraphBuilder::addOutPath(const Location & location, OutPath out_path) {
    addOutPath(out_paths_[location.getRow()][location.getColumn()], out_path);
}

void GraphBuilder::addOutPaths(const Location & location, std::initializer_list<OutPath> out_paths) {
    for (const auto & out_path : out_paths) {
        addOutPath(location, out_path);
    }
}

GraphBuilder & GraphBuilder::withStandardShiftLocations() noexcept {
    standard_shift_locations_ = true;
    return *this;
}

GraphBuilder & GraphBuilder::withLeftoverOutPaths(std::initializer_list<OutPath> out_paths) {
    for (const auto & out_path : out_paths) {
        addOutPath(leftover_out_paths_, out_path);
    }
    return *this;
}

MazeGraph GraphBuilder::constructGraph() {
    const auto extent = out_paths_.size();
    MazeGraph graph{extent};
    for (auto row = 0; row < extent; ++row) {
        for (auto column = 0; column < extent; ++column) {
            auto graph_out_paths = outPathsForMazeGraph(out_paths_[row][column]);
            graph.setOutPaths(Location{row, column}, graph_out_paths);
        }
    }
    auto leftover_out_paths = outPathsForMazeGraph(leftover_out_paths_);
    graph.setLeftoverOutPaths(leftover_out_paths);
    if (standard_shift_locations_) {
        for (auto pos = 1; pos < extent; pos += 2) {
            graph.addShiftLocation(Location{0, pos});
            graph.addShiftLocation(Location{extent - 1, pos});
            graph.addShiftLocation(Location{pos, 0});
            graph.addShiftLocation(Location{pos, extent - 1});
        }
    }
    return graph;
}

MazeGraph::OutPaths GraphBuilder::outPathsForMazeGraph(GraphBuilder::OutPaths out_paths) {
    MazeGraph::OutPathsIntegerType out_paths_int{0};
    for (OutPath out_path : {OutPath::North, OutPath::East, OutPath::South, OutPath::West}) {
        auto position = static_cast<size_t>(out_path);
        if (out_paths.test(position)) {
            out_paths_int += (1 << position);
        }
    }
    return static_cast<MazeGraph::OutPaths>(out_paths_int);
}

}
