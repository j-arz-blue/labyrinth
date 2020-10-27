#include "minimax.h"

#include "graph_algorithms.h"

#include "evaluators.h"
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

namespace solvers {

namespace minimax {

namespace { // anonymous namespace for file-internal linkage

OutPaths combineOutPaths(OutPaths out_paths1, OutPaths out_paths2) {
    return static_cast<OutPaths>(static_cast<OutPathsIntegerType>(out_paths1) |
                                 static_cast<OutPathsIntegerType>(out_paths2));
}

RotationDegreeType determineMaxRotation(OutPaths out_paths) {
    auto north_south = combineOutPaths(OutPaths::North, OutPaths::South);
    auto east_west = combineOutPaths(OutPaths::East, OutPaths::West);
    if (out_paths == north_south || out_paths == east_west) {
        return RotationDegreeType::_90;
    } else {
        return RotationDegreeType::_270;
    }
}

class ChildIterator {
public:
    // MazeGraph& graph, const Location& player_location, Location previous_shift_location)
    explicit ChildIterator(const GameTreeNode& parent) :
        parent_{parent},
        graph_{parent.getGraph()},
        player_location_{parent.getPlayerLocation()},
        invalid_shift_location_{opposingShiftLocation(parent_.getPreviousShiftLocation(), graph_.getExtent())},
        is_at_end_{false},
        current_rotation_{0},
        current_shift_location_{graph_.getShiftLocations().begin()} {
        skipInvalidShiftLocation();
        shift();
        initPossibleMoves();
    }

    PlayerAction getPlayerAction() const {
        return PlayerAction{ShiftAction{*current_shift_location_, current_rotation_}, *current_move_location_};
    }

    GameTreeNode createGameTreeNode() const {
        auto new_opponent_location =
            translateLocationByShift(parent_.getOpponentLocation(), *current_shift_location_, graph_.getExtent());
        return GameTreeNode{graph_, new_opponent_location, *current_move_location_, *current_shift_location_};
    }

    bool isAtEnd() const { return is_at_end_; }

    ChildIterator& operator++() {
        ++current_move_location_;
        if (current_move_location_ == possible_move_locations_.end()) {
            nextShift();
        }
        return *this;
    }

    ChildIterator operator++(int) {
        auto result = ChildIterator(*this);
        ++(*this);
        return result;
    }

private:
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

    const GameTreeNode& parent_;
    MazeGraph& graph_;
    Location player_location_;
    const Location invalid_shift_location_;
    bool is_at_end_;
    RotationDegreeType current_rotation_;
    RotationDegreeType pushed_out_rotation_;
    std::vector<Location>::const_iterator current_shift_location_;
    std::vector<Location> possible_move_locations_;
    std::vector<Location>::const_iterator current_move_location_;
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

    MinimaxResult runMinimax(const SolverInstance& solver_instance) {
        MazeGraph graph_copy{solver_instance.graph};
        GameTreeNode root{graph_copy,
                          solver_instance.player_location,
                          solver_instance.opponent_location,
                          solver_instance.previous_shift_location};
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
            return evaluator_.evaluate(node);
        }
        auto best_value = -infinity;
        for (ChildIterator child_iterator{node}; !child_iterator.isAtEnd(); ++child_iterator) {
            auto child_node = child_iterator.createGameTreeNode();
            auto negamax_value = -negamax(child_node, depth + 1);
            if (negamax_value > best_value) {
                best_value = negamax_value;
                if (depth == 0) {
                    best_action_ = child_iterator.getPlayerAction();
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
    IterativeDeepening(const Evaluator& evaluator) :
        evaluator_{evaluator}, max_depth_{0}, minimax_result_{error_player_action, -MinimaxRunner::infinity} {}
    PlayerAction iterateMinimax(const SolverInstance& solver_instance) {
        is_aborted = false;
        max_depth_ = 0;
        minimax_result_ = {error_player_action, -MinimaxRunner::infinity};
        MinimaxRunner runner{evaluator_, max_depth_};
        do {
            ++max_depth_;
            runner.setMaxDepth(max_depth_);
            auto new_result = runner.runMinimax(solver_instance);
            if (!is_aborted || max_depth_ == 1) {
                minimax_result_ = new_result;
            }
        } while (!minimax_result_.evaluation.is_terminal && !is_aborted);
        return minimax_result_.player_action;
    }

    size_t getCurrentSearchDepth() { return max_depth_; }

    bool currentResultIsTerminal() { return minimax_result_.evaluation.is_terminal; }

private:
    const Evaluator& evaluator_;
    size_t max_depth_;
    MinimaxResult minimax_result_;
};

std::list<IterativeDeepening> iterative_deepening_searches{};

} // namespace

inline bool operator>(const Evaluation& lhs, const Evaluation& rhs) noexcept {
    return lhs.value > rhs.value;
}

Evaluation operator-(const Evaluation& evaluation) noexcept {
    return Evaluation{-evaluation.value, evaluation.is_terminal};
}

Evaluation operator+(const Evaluation& evaluation1, const Evaluation& evaluation2) noexcept {
    return Evaluation{evaluation1.value + evaluation2.value, evaluation1.is_terminal || evaluation2.is_terminal};
}

Evaluation operator*(const Evaluation& evaluation, Evaluation::ValueType factor) noexcept {
    return Evaluation{evaluation.value * factor, evaluation.is_terminal};
}

MinimaxResult findBestAction(const SolverInstance& solver_instance, const Evaluator& evaluator, const size_t max_depth) {
    is_aborted = false;
    MinimaxRunner runner{evaluator, max_depth};
    return runner.runMinimax(solver_instance);
}

/**
 * Iterative Deepening implementation. Runs minimax with increasing depths.
 */
PlayerAction iterateMinimax(const SolverInstance& solver_instance, const Evaluator& evaluator) {
    iterative_deepening_searches.push_back(IterativeDeepening{evaluator});
    return iterative_deepening_searches.back().iterateMinimax(solver_instance);
}

void abortComputation() {
    is_aborted = true;
}

SearchStatus getSearchStatus() {
    auto search = iterative_deepening_searches.back();
    return SearchStatus{search.getCurrentSearchDepth(), search.currentResultIsTerminal()};
}

} // namespace minimax
} // namespace solvers
} // namespace labyrinth