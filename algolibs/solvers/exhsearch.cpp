#include "exhsearch.h"

#include "graph_algorithms.h"

#include "location.h"
#include "maze_graph.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <vector>

// The algorithm searches for a path reaching the objective in a tree of game states.
// For each analyzed succession of shift actions, it keeps track of all reachable locations.
// The root of this game tree is the initial graph given in the constructor.
// Each other game state is reached from its parent game state with a GameStateTransition,
// i.e. with a shift action and the set of then-reachable nodes.

// To be able to reconstruct the player actions,
// the reachable nodes have to include their source node in the previous game state
// Therefore, they are computed and stored as pairs, where the second entry is the NodeId of the reached node,
// and the first entry is the index of the source node in the respective parent array.

namespace labyrinth {

namespace solvers {

namespace exhsearch {

namespace { // anonymous namespace for file-internal linkage

struct GameStateNode;
using StatePtr = std::shared_ptr<GameStateNode>;

struct GameStateNode {
    explicit GameStateNode(StatePtr parent,
                           const ShiftAction& shift,
                           const std::vector<reachable::ReachableNode>& reached_nodes) :
        parent{parent}, shift{shift}, reached_nodes{reached_nodes} {}
    explicit GameStateNode() noexcept : parent{nullptr} {}

    StatePtr parent{nullptr};
    ShiftAction shift{};
    std::vector<reachable::ReachableNode> reached_nodes;

    bool isRoot() const noexcept { return parent == nullptr; }
};

using QueueType = std::queue<std::shared_ptr<GameStateNode>>;

MazeGraph createGraphFromState(const MazeGraph& base_graph, StatePtr current_state) {
    MazeGraph graph{base_graph};
    std::vector<ShiftAction> shifts;
    auto cur = current_state;
    while (!cur->isRoot()) {
        shifts.push_back(cur->shift);
        cur = cur->parent;
    }
    for (auto shift = shifts.rbegin(); shift != shifts.rend(); ++shift) {
        graph.shift(shift->location, shift->rotation);
    }
    return graph;
}

std::vector<Location> determineReachedLocations(const GameStateNode& current_state,
                                                const MazeGraph& graph,
                                                Location shift_location) {
    std::vector<Location> updated_player_locations;
    updated_player_locations.resize(current_state.reached_nodes.size());
    std::transform(current_state.reached_nodes.begin(),
                   current_state.reached_nodes.end(),
                   updated_player_locations.begin(),
                   [&graph, &shift_location](reachable::ReachableNode reached_node) {
                       const Location& reached_location = reached_node.reached_location;
                       return translateLocationByShift(reached_location, shift_location, graph.getExtent());
                   });
    return updated_player_locations;
}

StatePtr createNewState(const MazeGraph& shifted_graph, const ShiftAction& shift, StatePtr current_state) {
    auto updated_player_locations = determineReachedLocations(*current_state, shifted_graph, shift.location);
    StatePtr new_state = std::make_shared<GameStateNode>(
        current_state, shift, reachable::multiSourceReachableLocations(shifted_graph, updated_player_locations));
    return new_state;
}

std::vector<PlayerAction> reconstructActions(StatePtr new_state, size_t reachable_index) {
    auto cur = new_state;
    auto index = reachable_index;
    std::vector<PlayerAction> actions;
    while (!cur->isRoot()) {
        actions.push_back(PlayerAction{cur->shift, cur->reached_nodes[index].reached_location});
        index = cur->reached_nodes[index].parent_source_index;
        cur = cur->parent;
    }
    std::reverse(actions.begin(), actions.end());
    return actions;
}

OutPaths combineOutPaths(OutPaths out_paths1, OutPaths out_paths2) {
    return static_cast<OutPaths>(static_cast<OutPathsIntegerType>(out_paths1) |
                                 static_cast<OutPathsIntegerType>(out_paths2));
}

std::vector<RotationDegreeType> determineRotations(const Node& node) {
    auto north_south = combineOutPaths(OutPaths::North, OutPaths::South);
    auto east_west = combineOutPaths(OutPaths::East, OutPaths::West);
    if (node.out_paths == north_south || node.out_paths == east_west) {
        return std::vector<RotationDegreeType>{RotationDegreeType::_0, RotationDegreeType::_90};
    } else {
        return std::vector<RotationDegreeType>{
            RotationDegreeType::_0, RotationDegreeType::_90, RotationDegreeType::_180, RotationDegreeType::_270};
    }
}

MazeGraph shiftedGraph(const MazeGraph& base_graph, const ShiftAction& shift_action) {
    MazeGraph graph{base_graph};
    graph.shift(shift_action.location, shift_action.rotation);
    return graph;
}

} // anonymous namespace

void abortComputation() {
    is_aborted = true;
}

std::vector<PlayerAction> findBestActions(const SolverInstance& solver_instance) {
    // invariant: GameStateNode contains reachable nodes after shift has been carried out.
    is_aborted = false;
    auto objective_id = solver_instance.objective_id;
    QueueType state_queue;
    StatePtr root = std::make_shared<GameStateNode>();
    root->reached_nodes.emplace_back(0, solver_instance.player_location);
    root->shift = ShiftAction{solver_instance.previous_shift_location, RotationDegreeType::_0};
    state_queue.push(root);
    while (!state_queue.empty() && !is_aborted) {
        auto current_state = state_queue.front();
        state_queue.pop();
        MazeGraph current_graph = createGraphFromState(solver_instance.graph, current_state);
        auto shift_locations = current_graph.getShiftLocations();
        auto invalid_shift_location = opposingShiftLocation(current_state->shift.location, current_graph.getExtent());
        for (const auto& shift_location : shift_locations) {
            if (shift_location == invalid_shift_location) {
                continue;
            }
            auto rotations = determineRotations(current_graph.getLeftover());
            for (RotationDegreeType rotation : rotations) {
                const ShiftAction shift_action{shift_location, rotation};
                const MazeGraph shifted_graph = shiftedGraph(current_graph, shift_action);
                auto new_state = createNewState(shifted_graph, shift_action, current_state);
                auto found_objective =
                    std::find_if(new_state->reached_nodes.begin(),
                                 new_state->reached_nodes.end(),
                                 [objective_id, &shifted_graph](auto& reached_node) {
                                     return shifted_graph.getNode(reached_node.reached_location).node_id == objective_id;
                                 });
                if (found_objective != new_state->reached_nodes.end()) {
                    const size_t reachable_index = found_objective - new_state->reached_nodes.begin();
                    return reconstructActions(new_state, reachable_index);
                } else {
                    state_queue.push(new_state);
                }
            }
        }
    }
    return std::vector<PlayerAction>{};
}

} // namespace exhsearch
} // namespace solvers
} // namespace labyrinth
