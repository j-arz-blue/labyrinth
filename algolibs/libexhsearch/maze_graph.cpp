#include "maze_graph.h"
#include "location.h"
#include <algorithm>
#include <array>

namespace labyrinth {

namespace { // anonymous namespace for file-internal linkage

constexpr MazeGraph::OutPaths rotateOutPaths(MazeGraph::OutPaths out_paths, MazeGraph::RotationDegreeType rotation) noexcept {
    const MazeGraph::OutPathsIntegerType mask = 15;
    const auto rotations = (rotation / 90 + 4) % 4;
    auto out_paths_integer = static_cast<MazeGraph::OutPathsIntegerType>(out_paths);
    out_paths_integer = out_paths_integer << rotations | out_paths_integer >> (4 - rotations);
    out_paths_integer &= mask;
    return static_cast<MazeGraph::OutPaths>(out_paths_integer);
}

constexpr MazeGraph::OutPaths mirrorOutPath(MazeGraph::OutPaths out_path) noexcept {
    return rotateOutPaths(out_path, 180);
}

Location::OffsetType offsetFromOutPath(MazeGraph::OutPaths out_path) noexcept {
    switch (out_path) {
    case MazeGraph::OutPaths::North:
        return Location::OffsetType{-1, 0};
    case MazeGraph::OutPaths::East:
        return Location::OffsetType{0, 1};
    case MazeGraph::OutPaths::South:
        return Location::OffsetType{1, 0};
    case MazeGraph::OutPaths::West:
        return Location::OffsetType{0, -1};
    default:
        return Location::OffsetType{0, 0};
    }
}

} // anonymous namespace

MazeGraph::MazeGraph(size_t extent) : extent_{extent} {
    NodeId current = 0;
    node_matrix_.resize(extent * extent);
    for (auto row = 0; row < extent; row++) {
        for (auto column = 0; column < extent; column++) {
            getNode(Location{row, column}).node_id = current++;
        }
    }
    leftover_.node_id = current;
}

MazeGraph::MazeGraph(size_t extent, std::vector<InputNode> nodes) : extent_{extent} {
    auto current_input = nodes.begin();
    node_matrix_.resize(extent * extent);
    for (auto row = 0; row < extent; row++) {
        for (auto column = 0; column < extent; column++) {
            auto & node = getNode(Location{row, column});
            node.node_id = current_input->node_id;
            node.rotation = current_input->rotation;
            node.out_paths = current_input->out_paths;
            ++current_input;
        }
    }
    leftover_.node_id = current_input->node_id;
    leftover_.rotation = current_input->rotation;
    leftover_.out_paths = current_input->out_paths;
}

void MazeGraph::setOutPaths(const Location & location, OutPaths out_paths) {
    getNode(location).out_paths = out_paths;
}

void MazeGraph::addShiftLocation(const Location & location) {
    if (std::find(shift_locations_.begin(), shift_locations_.end(), location) == shift_locations_.end()) {
        shift_locations_.push_back(location);
    }
}

void MazeGraph::setLeftoverOutPaths(OutPaths out_paths) {
    leftover_.out_paths = out_paths;
}

bool MazeGraph::hasOutPath(const Location & location, OutPaths out_path) const {
    return hasOutPath(getNode(location), out_path);
}

MazeGraph::NodeId MazeGraph::getNodeId(const Location & location) const {
    return getNode(location).node_id;
}

MazeGraph::NodeId MazeGraph::getLeftoverNodeId() const noexcept {
    return leftover_.node_id;
}

bool MazeGraph::leftoverHasOutPath(OutPaths out_path) const {
    return hasOutPath(leftover_, out_path);
}

Location MazeGraph::getLocation(MazeGraph::NodeId node_id, const Location & leftover_location) const {
    Location result = leftover_location;
    for (Location::IndexType row = 0; row < extent_; ++row) {
        for (Location::IndexType column = 0; column < extent_; ++column) {
            Location location{row, column};
            if (getNode(location).node_id == node_id) {
                return location;
            }
        }
    }
    return leftover_location;
}

MazeGraph::Neighbors MazeGraph::neighbors(const Location & location) const {
    return MazeGraph::Neighbors(*this, location, getNode(location));
}

size_t MazeGraph::getNumberOfNodes() const noexcept {
    return extent_ * extent_;
}

size_t MazeGraph::getExtent() const noexcept {
    return extent_;
}

void MazeGraph::shift(const Location & location, RotationDegreeType leftoverRotation) {
    OffsetType::OffsetValueType row_offset{0}, column_offset{0};
    if (location.getRow() == 0) {
        row_offset = 1;
    }
    else if (location.getRow() == extent_ - 1) {
        row_offset = -1;
    }
    else if (location.getColumn() == 0) {
        column_offset = 1;
    }
    else if (location.getColumn() == extent_ - 1) {
        column_offset = -1;
    }
    const OffsetType offset{row_offset, column_offset};
    std::vector<Location> line{};
    line.reserve(extent_);
    Location current = location;
    for (size_t i = 0; i < extent_; ++i) {
        line.push_back(current);
        current = current + offset;
    }
    const Node updated_leftover = getNode(line.back());
    for (auto to = line.rbegin(), from = std::next(to); from != line.rend(); ++to, ++from) {
        getNode(*to) = getNode(*from);
    }
    leftover_.rotation = leftoverRotation;
    getNode(line.front()) = leftover_;
    leftover_ = updated_leftover;
}

const MazeGraph::Node & MazeGraph::getNode(const Location & location) const {
    return node_matrix_[location.getRow() * extent_ + location.getColumn()];
}

MazeGraph::Node & MazeGraph::getNode(const Location & location) {
    return node_matrix_[location.getRow() * extent_ + location.getColumn()];
}

bool MazeGraph::hasOutPath(const Node & node, OutPaths out_path) const {
    auto out_path_to_check = rotateOutPaths(out_path, -node.rotation);
    return static_cast<MazeGraph::OutPathsIntegerType>(node.out_paths)
           & static_cast<MazeGraph::OutPathsIntegerType>(out_path_to_check);
}

bool MazeGraph::isInside(const Location & location) const noexcept {
    return (location.getRow() >= 0) &&
        (location.getColumn() >= 0) &&
        (location.getRow() < extent_) &&
        (location.getColumn() < extent_);
}

MazeGraph::NeighborIterator MazeGraph::NeighborIterator::begin(const MazeGraph & graph, const Location & location, const Node & node){
    return MazeGraph::NeighborIterator{OutPath::North, graph, location, node};
}

MazeGraph::NeighborIterator MazeGraph::NeighborIterator::end(const MazeGraph & graph, const Location & location, const Node & node) {
    auto sentinel = static_cast<OutPath>(static_cast<OutPathsIntegerType>(OutPath::West) << 1);
    return MazeGraph::NeighborIterator{sentinel, graph, location, node};
}

bool MazeGraph::NeighborIterator::operator==(const NeighborIterator & other) const noexcept {
    return current_out_path_ == other.current_out_path_;
}

bool MazeGraph::NeighborIterator::operator!=(const NeighborIterator & other) const noexcept {
    return !(*this == other);
}

MazeGraph::NeighborIterator::reference MazeGraph::NeighborIterator::operator*() const {
    return location_ + offsetFromOutPath(current_out_path_);
}

MazeGraph::NeighborIterator & MazeGraph::NeighborIterator::operator++() {
    current_out_path_ = static_cast<OutPath>(static_cast<OutPathsIntegerType>(current_out_path_) << 1);
    moveToNextNeighbor();
    return *this;
}

MazeGraph::NeighborIterator MazeGraph::NeighborIterator::operator++(int) {
    auto result = NeighborIterator(*this);
    ++(*this);
    return result;
}

void MazeGraph::NeighborIterator::moveToNextNeighbor() {
    static const auto sentinel = static_cast<OutPathsIntegerType>(OutPath::West) << 1;
    const auto rotated_out_paths = rotateOutPaths(node_.out_paths, node_.rotation);
    const auto node_out_paths = static_cast<OutPathsIntegerType>(rotated_out_paths);
    auto out_path_int = static_cast<OutPathsIntegerType>(current_out_path_);
    
    while ((out_path_int < sentinel)
        && (!(out_path_int & node_out_paths)
        || !isNeighbor(current_out_path_))) {
        out_path_int <<= 1;
        current_out_path_ = static_cast<OutPath>(out_path_int);
    }
}

bool MazeGraph::NeighborIterator::isNeighbor(OutPath out_path) {
    const auto potential_location = location_ + offsetFromOutPath(out_path);
    return graph_.isInside(potential_location) && graph_.hasOutPath(graph_.getNode(potential_location), mirrorOutPath(out_path));
}

MazeGraph::Neighbors::Neighbors(const MazeGraph & graph, const Location & location, const Node & node) noexcept :
    graph_{graph}, location_{location}, node_{node} {}

MazeGraph::NeighborIterator MazeGraph::Neighbors::begin() {
    return MazeGraph::NeighborIterator::begin(graph_, location_, node_);
}

MazeGraph::NeighborIterator MazeGraph::Neighbors::end() {
    return MazeGraph::NeighborIterator::end(graph_, location_, node_);
}

} // namespace graph

namespace std {

std::ostream & operator<<(std::ostream & os, const labyrinth::MazeGraph & graph) {
    const auto extent = graph.getExtent();
    std::string row_delimiter(extent * 4, '-');
    for (size_t row = 0; row < extent; row++) {
        std::array<std::string, 3> lines = {std::string(extent * 4, '#'), std::string(extent * 4, '#'), std::string(extent * 4, '#')};
        for (size_t column = 0; column < extent; column++) {
            if (graph.hasOutPath(labyrinth::Location(row, column), labyrinth::MazeGraph::OutPaths::North)) {
                lines[0][column * 4 + 1] = '.';
            }
            if (graph.hasOutPath(labyrinth::Location(row, column), labyrinth::MazeGraph::OutPaths::East)) {
                lines[1][column * 4 + 2] = '.';
            }
            if (graph.hasOutPath(labyrinth::Location(row, column), labyrinth::MazeGraph::OutPaths::South)) {
                lines[2][column * 4 + 1] = '.';
            }
            if (graph.hasOutPath(labyrinth::Location(row, column), labyrinth::MazeGraph::OutPaths::West)) {
                lines[1][column * 4 + 0] = '.';
            }
            lines[1][column * 4 + 1] = '.';
            for (auto & line : lines) {
                line[column * 4 + 3] = '|';
            }
        }
        for (const auto & line : lines) {
            os << line << "\n";
        }
        os << row_delimiter << "\n";
    }
    os << std::endl;
    return os;
}
}

