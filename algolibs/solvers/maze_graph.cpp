#include "maze_graph.h"
#include "location.h"
#include <algorithm>
#include <array>
#include <vector>

namespace labyrinth {

namespace { // anonymous namespace for file-internal linkage

constexpr OutPaths rotateOutPaths(OutPaths out_paths, RotationDegreeType rotation) noexcept {
    const OutPathsIntegerType mask = 15;
    const auto rotations = static_cast<RotationDegreeIntegerType>(rotation);
    auto out_paths_integer = static_cast<OutPathsIntegerType>(out_paths);
    out_paths_integer = out_paths_integer << rotations | out_paths_integer >> (4 - rotations);
    out_paths_integer &= mask;
    return static_cast<OutPaths>(out_paths_integer);
}

constexpr OutPaths mirrorOutPath(OutPaths out_path) noexcept {
    return rotateOutPaths(out_path, RotationDegreeType::_180);
}

Location::OffsetType offsetFromOutPath(OutPaths out_path) noexcept {
    switch (out_path) {
    case OutPaths::North:
        return Location::OffsetType{-1, 0};
    case OutPaths::East:
        return Location::OffsetType{0, 1};
    case OutPaths::South:
        return Location::OffsetType{1, 0};
    case OutPaths::West:
        return Location::OffsetType{0, -1};
    default:
        return Location::OffsetType{0, 0};
    }
}

size_t integerSquareRoot(size_t number) {
    size_t root = 0;
    while (root * root <= number) {
        root++;
    }
    return root - 1;
}

RotationDegreeType operator-(RotationDegreeType rotation) {
    return static_cast<RotationDegreeType>(4 - static_cast<RotationDegreeIntegerType>(rotation));
}

} // anonymous namespace

RotationDegreeType nextRotation(RotationDegreeType rotation) {
    return static_cast<RotationDegreeType>(static_cast<RotationDegreeIntegerType>(rotation) + 1);
}

bool hasOutPath(const Node& node, OutPaths out_path) {
    const auto out_path_to_check = rotateOutPaths(out_path, -node.rotation);
    return static_cast<OutPathsIntegerType>(node.out_paths) & static_cast<OutPathsIntegerType>(out_path_to_check);
}

MazeGraph::MazeGraph(ExtentType extent) : size_{static_cast<SizeType>(extent * extent)}, extent_{extent} {
    NodeId current = 0;
    node_matrix_.resize(size_);
    for (auto row = 0; row < extent_; row++) {
        for (auto column = 0; column < extent_; column++) {
            getNode(Location{row, column}).node_id = current++;
        }
    }
    leftover_.node_id = current;
}

MazeGraph::MazeGraph(const std::vector<Node>& nodes) :
    size_{nodes.size() - 1}, extent_{static_cast<ExtentType>(integerSquareRoot(nodes.size()))} {
    auto current_input = nodes.begin();
    node_matrix_.resize(size_);
    for (auto row = 0; row < extent_; row++) {
        for (auto column = 0; column < extent_; column++) {
            auto& node = getNode(Location{row, column});
            node = *current_input;
            ++current_input;
        }
    }
    leftover_ = *current_input;
}

void MazeGraph::setOutPaths(const Location& location, OutPaths out_paths) {
    getNode(location).out_paths = out_paths;
}

void MazeGraph::addShiftLocation(const Location& location) {
    if (std::find(shift_locations_.begin(), shift_locations_.end(), location) == shift_locations_.end()) {
        shift_locations_.push_back(location);
    }
}

void MazeGraph::setLeftoverOutPaths(OutPaths out_paths) {
    leftover_.out_paths = out_paths;
}

Location MazeGraph::getLocation(NodeId node_id, const Location& leftover_location) const {
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

MazeGraph::NeighborIterator MazeGraph::neighbors(const Location& location) const {
    return MazeGraph::NeighborIterator(OutPaths::North, *this, location, getNode(location));
}

MazeGraph::SizeType MazeGraph::getNumberOfNodes() const noexcept {
    return size_ + 1;
}

MazeGraph::ExtentType MazeGraph::getExtent() const noexcept {
    return extent_;
}

void MazeGraph::shift(const Location& location, RotationDegreeType leftover_rotation) {
    const OffsetType offset = getOffsetByShiftLocation(location, extent_);
    auto to_location = opposingShiftLocation(location, extent_);
    const Node updated_leftover = getNode(to_location);
    for (auto i = 0; i < extent_ - 1; ++i) {
        auto from_location = to_location - offset;
        getNode(to_location) = getNode(from_location);
        to_location = from_location;
    }
    leftover_.rotation = leftover_rotation;
    getNode(to_location) = leftover_;
    leftover_ = updated_leftover;
}

Location MazeGraph::NeighborIterator::operator*() const {
    return location_ + offsetFromOutPath(current_out_path_);
}

MazeGraph::NeighborIterator& MazeGraph::NeighborIterator::operator++() {
    current_out_path_ = static_cast<OutPaths>(static_cast<OutPathsIntegerType>(current_out_path_) << 1);
    moveToNextNeighbor();
    return *this;
}

MazeGraph::NeighborIterator MazeGraph::NeighborIterator::operator++(int) {
    auto result = NeighborIterator(*this);
    ++(*this);
    return result;
}

void MazeGraph::NeighborIterator::moveToNextNeighbor() {

    const auto rotated_out_paths = rotateOutPaths(node_.out_paths, node_.rotation);
    const auto node_out_paths = static_cast<OutPathsIntegerType>(rotated_out_paths);
    auto out_path_int = static_cast<OutPathsIntegerType>(current_out_path_);

    while ((out_path_int < sentinel_) && (!(out_path_int & node_out_paths) || !isNeighbor(current_out_path_))) {
        out_path_int <<= 1;
        current_out_path_ = static_cast<OutPaths>(out_path_int);
    }
}

bool MazeGraph::NeighborIterator::isAtEnd() const {
    auto out_path_int = static_cast<OutPathsIntegerType>(current_out_path_);
    return out_path_int >= sentinel_;
}

bool MazeGraph::NeighborIterator::isNeighbor(OutPaths out_path) const {
    const auto potential_location = location_ + offsetFromOutPath(out_path);
    return graph_.isInside(potential_location) && hasOutPath(graph_.getNode(potential_location), mirrorOutPath(out_path));
}

Location opposingShiftLocation(const Location& location, MazeGraph::ExtentType extent) noexcept {
    const auto row = location.getRow();
    const auto column = location.getColumn();
    MazeGraph::ExtentType border = extent - 1;
    if (column == 0) {
        return Location{row, border};
    } else if (row == 0) {
        return Location{border, column};
    } else if (column == border) {
        return Location{row, 0};
    } else if (row == border) {
        return Location{0, column};
    }
    return location;
}

Location translateLocationByShift(const Location& location,
                                  const Location& shift_location,
                                  MazeGraph::ExtentType extent) noexcept {
    const Location::OffsetType offset = getOffsetByShiftLocation(shift_location, extent);
    if (0 != offset.row_offset) { // shift in direction N or S
        if (location.getColumn() == shift_location.getColumn()) {
            const Location::IndexType row = (location.getRow() + offset.row_offset + extent) % extent;
            const Location::IndexType column = location.getColumn();
            return Location{row, column};
        }
    } else { // shift in direction E or W
        if (location.getRow() == shift_location.getRow()) {
            const Location::IndexType row = location.getRow();
            const Location::IndexType column = (location.getColumn() + offset.column_offset + extent) % extent;
            return Location{row, column};
        }
    }
    return location;
}

Location::OffsetType getOffsetByShiftLocation(const Location& shift_location, MazeGraph::ExtentType extent) noexcept {
    Location::OffsetType::OffsetValueType row_offset{0}, column_offset{0};
    if (shift_location.getRow() == 0) {
        row_offset = 1;
    } else if (shift_location.getRow() == extent - 1) {
        row_offset = -1;
    } else if (shift_location.getColumn() == 0) {
        column_offset = 1;
    } else if (shift_location.getColumn() == extent - 1) {
        column_offset = -1;
    }
    return Location::OffsetType{row_offset, column_offset};
}

} // namespace labyrinth

namespace std {

std::ostream& operator<<(std::ostream& os, const labyrinth::MazeGraph& graph) {
    const auto extent = graph.getExtent();
    std::string row_delimiter(extent * 4, '-');
    for (auto row = 0; row < extent; row++) {
        std::array<std::string, 3> lines = {
            std::string(extent * 4, '#'), std::string(extent * 4, '#'), std::string(extent * 4, '#')};
        for (auto column = 0; column < extent; column++) {
            auto node = graph.getNode(labyrinth::Location(row, column));
            if (labyrinth::hasOutPath(node, labyrinth::OutPaths::North)) {
                lines[0][column * 4 + 1] = '.';
            }
            if (labyrinth::hasOutPath(node, labyrinth::OutPaths::East)) {
                lines[1][column * 4 + 2] = '.';
            }
            if (labyrinth::hasOutPath(node, labyrinth::OutPaths::South)) {
                lines[2][column * 4 + 1] = '.';
            }
            if (labyrinth::hasOutPath(node, labyrinth::OutPaths::West)) {
                lines[1][column * 4 + 0] = '.';
            }
            lines[1][column * 4 + 1] = '.';
            for (auto& line : lines) {
                line[column * 4 + 3] = '|';
            }
        }
        for (const auto& line : lines) {
            os << line << "\n";
        }
        os << row_delimiter << "\n";
    }
    os << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, labyrinth::RotationDegreeType rotation) {
    os << (static_cast<labyrinth::RotationDegreeIntegerType>(rotation) * 90) << "°";
    return os;
}
} // namespace std
