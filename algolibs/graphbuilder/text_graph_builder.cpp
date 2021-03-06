#include "text_graph_builder.h"

namespace labyrinth {

MazeGraph TextGraphBuilder::buildGraph() {
    const size_t extent{lines_.size() / lines_per_node};
    out_paths_.resize(extent);
    for (auto& row : out_paths_) {
        row.resize(extent);
    }
    for (auto row = 0u; row < extent; row++) {
        for (auto column = 0u; column < extent; column++) {
            if (lines_[first(row)][second(column)] == '.') {
                addOutPath(Location{row, column}, OutPathPosition::North);
            }
            if (lines_[second(row)][third(column)] == '.') {
                addOutPath(Location{row, column}, OutPathPosition::East);
            }
            if (lines_[third(row)][second(column)] == '.') {
                addOutPath(Location{row, column}, OutPathPosition::South);
            }
            if (lines_[second(row)][first(column)] == '.') {
                addOutPath(Location{row, column}, OutPathPosition::West);
            }
        }
    }
    return constructGraph();
}

TextGraphBuilder& TextGraphBuilder::setMaze(const std::vector<std::string>& lines) {
    lines_ = lines;
    return *this;
}

} // namespace labyrinth
