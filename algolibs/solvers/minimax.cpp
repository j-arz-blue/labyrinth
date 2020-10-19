#include "minimax.h"

#include "graph_algorithms.h"

#include "location.h"
#include "maze_graph.h"

#include <algorithm>
#include <list>
#include <memory>
#include <optional>

/**
 * The minimax algorithm searches for the optimal action to play in a two-player zero-sum game.
 * It traverses the game tree and assigns a value to each node (=state of the game).
 * The algorithm recursively searches for the action which maximizes the minimum values of the opponent's possible following moves.
 *
 * This implementation is divided into four parts:
 * - The GameTreeNode class contains the labyrinth game logic. It allows iterating over the possible moves.
 * - The Evaluator determines a value for a given GameTreeNode.
 * - The negamax implementation in MinimaxRunner traverses the game tree by creating GameTreeNodes.
 * - The iterative deepening algorithm iteratively calls the minimax algorithm with increasing depths.
 */

namespace labyrinth {

namespace minimax {

namespace { // anonymous namespace for file-internal linkage

/**
 * Represents a node in the game tree for two players.
 *
 * Each node represents a current state of the maze. As minimax algorithm is essentially a depth-first search, it is ok to
 * store this MazeGraph instance explicitly.
 * A node has a reference to a PlayerAction.
 * The state of the board is the result of applying this action on the parent's board.
 * A node offers an iterator over all it's possible next PlayerActions.
 * From the viewpoint of a GameTreeNode, it is always player 0 who performs the action.
 * Therefore, the player locations are swapped in the constructor.
 */
class GameTreeNode {
private:
    class PlayerActionIterator;

public:
    explicit GameTreeNode(MazeGraph& graph, const PlayerAction& previous_player_action, const Location& player_location) :
        graph_{graph},
        player_locations_{player_location, previous_player_action.move_location},
        previous_shift_location_{previous_player_action.shift.location} {}

    /// Returns an iterator over possible PlayerActions.
    PlayerActionIterator possibleActions() const {
        return PlayerActionIterator(graph_, player_locations_[0], previous_shift_location_);
    }

    const MazeGraph& getGraph() const noexcept { return graph_; }

    const Location& getPlayerLocation() const { return player_locations_[0]; }

    const Location& getOpponentLocation() const { return player_locations_[1]; }

private:
    class PlayerActionIterator {
    public:
        explicit PlayerActionIterator(MazeGraph& graph, const Location& player_location, Location previous_shift_location) :
            graph_{graph},
            player_location_{player_location},
            is_at_end_{false},
            current_rotation_{0},
            current_shift_location_{graph_.getShiftLocations().begin()} {
            invalid_shift_location_ = opposingShiftLocation(previous_shift_location, graph_.getExtent());
            skipInvalidShiftLocation();
            shift();
            initPossibleMoves();
        }

        PlayerAction getPlayerAction() const {
            return PlayerAction{ShiftAction{*current_shift_location_, current_rotation_}, *current_move_location_};
        }

        MazeGraph& getGraph() { return graph_; }

        bool isAtEnd() const { return is_at_end_; }

        PlayerActionIterator& operator++() {
            ++current_move_location_;
            if (current_move_location_ == possible_move_locations_.end()) {
                nextShift();
            }
            return *this;
        }

        PlayerActionIterator operator++(int) {
            auto result = PlayerActionIterator(*this);
            ++(*this);
            return result;
        }

    private:
        OutPaths combineOutPaths(OutPaths out_paths1, OutPaths out_paths2) const {
            return static_cast<OutPaths>(static_cast<OutPathsIntegerType>(out_paths1) |
                                         static_cast<OutPathsIntegerType>(out_paths2));
        }

        RotationDegreeType determineMaxRotation(OutPaths out_paths) const {
            auto north_south = combineOutPaths(OutPaths::North, OutPaths::South);
            auto east_west = combineOutPaths(OutPaths::East, OutPaths::West);
            if (out_paths == north_south || out_paths == east_west) {
                return RotationDegreeType::_90;
            } else {
                return RotationDegreeType::_270;
            }
        }

        void nextShift() {
            // expects the graph to be (still) shifted.
            // At the end, the graph is either (again) shifted, or it is unshifted and is_at_end is true
            auto max_rotation = determineMaxRotation(graph_.getNode(*current_shift_location_).out_paths);
            if (current_rotation_ < max_rotation) {
                current_rotation_ = nextRotation(current_rotation_);
                graph_.getNode(*current_shift_location_).rotation = current_rotation_;
            } else {
                undoShift();
                current_rotation_ = RotationDegreeType::_0;
                ++current_shift_location_;
                if (current_shift_location_ != graph_.getShiftLocations().end()) {
                    skipInvalidShiftLocation();
                }
                if (current_shift_location_ == graph_.getShiftLocations().end()) {
                    is_at_end_ = true;
                } else {
                    shift();
                }
            }
            initPossibleMoves();
        }

        void skipInvalidShiftLocation() {
            if (invalid_shift_location_ == *current_shift_location_) {
                ++current_shift_location_;
            }
        }

        void shift() {
            graph_.shift(*current_shift_location_, current_rotation_);
            pushed_out_rotation_ = graph_.getLeftover().rotation;
            player_location_ = translateLocationByShift(player_location_, *current_shift_location_, graph_.getExtent());
        }

        void undoShift() {
            auto opposing_shift_location = opposingShiftLocation(*current_shift_location_, graph_.getExtent());
            graph_.shift(opposing_shift_location, pushed_out_rotation_);
            player_location_ = translateLocationByShift(player_location_, opposing_shift_location, graph_.getExtent());
        }

        void initPossibleMoves() {
            // expects the graph to already be shifted
            if (!is_at_end_) {
                possible_move_locations_ = reachable::reachableLocations(graph_, player_location_);
            } else {
                possible_move_locations_.resize(0);
            }
            current_move_location_ = possible_move_locations_.begin();
        }

        MazeGraph& graph_;
        Location player_location_;
        Location invalid_shift_location_;
        bool is_at_end_;
        RotationDegreeType current_rotation_;
        RotationDegreeType pushed_out_rotation_;
        std::vector<Location>::const_iterator current_shift_location_;
        std::vector<Location> possible_move_locations_;
        std::vector<Location>::const_iterator current_move_location_;
    };

    MazeGraph& graph_;
    std::array<Location, 2> player_locations_;
    Location previous_shift_location_;
};

inline bool operator>(const Evaluation& lhs, const Evaluation& rhs) noexcept {
    return lhs.value > rhs.value;
}

Evaluation operator-(const Evaluation& evaluation) noexcept {
    return Evaluation{-evaluation.value, evaluation.is_terminal};
}

/**
 * Evaluates a GameTreeNode for the minimax algorithm.
 *
 * Returns a value and a flag if the objective was reached.
 */
class Evaluator {
public:
    using Evaluation = minimax::Evaluation;
    explicit Evaluator(NodeId objective_id) : objective_id_{objective_id} {}

    /* The implementation of negamax does not use an alternating player index.
     * Therefore, the Evaluator always has to evaluate from the viewpoint of player 0.
     * For a GameTreeNode, the incoming player action is the opponent action. Hence,
     * The non-heuristic Evaluator will return -1 if the opponent has reached the objective.
     * However, if the current player is placed on the objective, it has to return 0 (not 1), because
     * The player has not actively reached the objective in his last move.
     */
    Evaluation evaluate(const GameTreeNode& node) const {
        if (node.getGraph().getNode(node.getOpponentLocation()).node_id == objective_id_) {
            return Evaluation{-1, true};
        } else {
            return 0;
        }
    }

private:
    NodeId objective_id_;
};

/**
 * Encapsulates the negamax implementation with its required data.
 * Is able to store data between consecutive negamax runs.
 */
class MinimaxRunner {
public:
    constexpr static Evaluation infinity{10000};

    explicit MinimaxRunner(const Evaluator& evaluator, size_t max_depth) :
        evaluator_{evaluator}, max_depth_{max_depth}, best_action_{error_player_action} {}

    MinimaxResult runMinimax(const MazeGraph& graph,
                             const Location& player_location,
                             const Location& opponent_location,
                             const Location& previous_shift_location = Location{-1, -1}) {
        MazeGraph graph_copy{graph};
        const auto previous_action = PlayerAction{
            ShiftAction{previous_shift_location, graph.getNode(previous_shift_location).rotation}, opponent_location};
        GameTreeNode root{graph_copy, previous_action, player_location};
        const auto& evaluation = negamax(root);
        return MinimaxResult{best_action_, evaluation};
    }

    /**
     * This implementation of negamax does not use an alternating player index.
     * Therefore, the Evaluator always has to evaluate from the viewpoint of player 0.
     */
    Evaluation negamax(const GameTreeNode& node, size_t depth = 0) {
        auto evaluation = evaluator_.evaluate(node);
        if (depth == max_depth_ or evaluation.is_terminal) {
            return evaluation;
        }
        auto best_value = -infinity;
        for (auto action_iterator = node.possibleActions(); !action_iterator.isAtEnd(); ++action_iterator) {
            auto action = action_iterator.getPlayerAction();
            auto& graph = action_iterator.getGraph();
            auto new_opponent_location =
                translateLocationByShift(node.getOpponentLocation(), action.shift.location, graph.getExtent());
            GameTreeNode child_node{graph, action, new_opponent_location};
            auto negamax_value = -negamax(child_node, depth + 1);
            if (negamax_value > best_value) {
                best_value = negamax_value;
                if (depth == 0) {
                    best_action_ = action;
                }
            }
            if (is_aborted) {
                break;
            }
        }
        return best_value;
    }

    void setMaxDepth(size_t depth) { max_depth_ = depth; }

    const PlayerAction& getBestAction() const { return best_action_; }

private:
    const Evaluator& evaluator_;
    size_t max_depth_;
    PlayerAction best_action_;
};

class IterativeDeepening {
public:
    IterativeDeepening() : max_depth{0}, minimax_result{error_player_action, -MinimaxRunner::infinity} {}
    PlayerAction iterateMinimax(const MazeGraph& graph,
                                const Location& player_location,
                                const Location& opponent_location,
                                const NodeId objective_id,
                                const Location& previous_shift_location) {
        is_aborted = false;
        max_depth = 0;
        minimax_result = {error_player_action, -MinimaxRunner::infinity};
        Evaluator evaluator{objective_id};
        MinimaxRunner runner{evaluator, max_depth};
        do {
            ++max_depth;
            runner.setMaxDepth(max_depth);
            auto new_result = runner.runMinimax(graph, player_location, opponent_location, previous_shift_location);
            if (!is_aborted || max_depth == 1) {
                minimax_result = new_result;
            }
        } while (!minimax_result.evaluation.is_terminal && !is_aborted);
        return minimax_result.player_action;
    }

    size_t getCurrentSearchDepth() { return max_depth; }

    bool currentResultIsTerminal() { return minimax_result.evaluation.is_terminal; }

private:
    size_t max_depth;
    MinimaxResult minimax_result;
};

std::list<IterativeDeepening> iterative_deepening_searches{};

} // namespace

MinimaxResult findBestAction(const MazeGraph& graph,
                             const Location& player_location,
                             const Location& opponent_location,
                             const NodeId objective_id,
                             const size_t max_depth,
                             const Location& previous_shift_location) {
    is_aborted = false;
    Evaluator evaluator{objective_id};
    MinimaxRunner runner{evaluator, max_depth};
    return runner.runMinimax(graph, player_location, opponent_location, previous_shift_location);
}

/**
 * Iterative Deepening implementation. Runs minimax with increasing depths.
 */
PlayerAction iterateMinimax(const MazeGraph& graph,
                            const Location& player_location,
                            const Location& opponent_location,
                            const NodeId objective_id,
                            const Location& previous_shift_location) {
    iterative_deepening_searches.push_back(IterativeDeepening{});
    return iterative_deepening_searches.back().iterateMinimax(
        graph, player_location, opponent_location, objective_id, previous_shift_location);
}

void abortComputation() {
    is_aborted = true;
}

SearchStatus getSearchStatus() {
    auto search = iterative_deepening_searches.back();
    return SearchStatus{search.getCurrentSearchDepth(), search.currentResultIsTerminal()};
}

} // namespace minimax
} // namespace labyrinth