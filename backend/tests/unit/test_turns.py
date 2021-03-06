""" Tests Turns of game.py """
from unittest.mock import Mock
from datetime import timedelta
import time

import pytest


from labyrinth.model.exceptions import TurnActionViolationException
from labyrinth.model.game import Turns, PlayerAction, Player


def test_next_player_action_should_be_none_without_players():
    """ Tests next_player_action """
    turns = Turns()
    assert turns.next_player_action() is None


def test_next_player_action_with_one_player_should_be_shift():
    """ Tests next_player_action """
    player = Player(7, 0)
    turns = Turns(players=[player])
    assert turns.next_player_action() == PlayerAction(player, PlayerAction.SHIFT_ACTION)


def test_next_player_action_after_perform_shift_should_be_move():
    """ Tests next_player_action and perform_action """
    player = Player(7, 0)
    turns = Turns(players=[player])
    turns.perform_action(player, PlayerAction.SHIFT_ACTION)
    assert turns.next_player_action() == PlayerAction(player, PlayerAction.MOVE_ACTION)


def test_perform_invalid_action_should_raise():
    """ Tests perform_action """
    player1, player2 = Player(7, 0), Player(11, 0)
    turns = Turns(players=[player1, player2])
    with pytest.raises(TurnActionViolationException):
        turns.perform_action(player2, PlayerAction.SHIFT_ACTION)


def test_perform_invalid_action_does_not_alter_turns():
    """ Tests perform_action """
    player1, player2 = Player(7, 0), Player(11, 0)
    turns = Turns(players=[player1, player2])
    with pytest.raises(TurnActionViolationException):
        turns.perform_action(player2, PlayerAction.SHIFT_ACTION)
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    turns.perform_action(player1, PlayerAction.MOVE_ACTION)
    turns.perform_action(player2, PlayerAction.SHIFT_ACTION)
    with pytest.raises(TurnActionViolationException):
        turns.perform_action(player2, PlayerAction.SHIFT_ACTION)


def test_is_action_possible_with_next_action():
    """ Tests is_action_possible and constructor parameter next_action """
    players = [Player(id, 0) for id in [9, 0, 3]]
    turns = Turns(players=players, next_action=PlayerAction(players[2], PlayerAction.MOVE_ACTION))
    assert not turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)
    assert turns.is_action_possible(players[2], PlayerAction.MOVE_ACTION)
    turns.perform_action(players[2], PlayerAction.MOVE_ACTION)
    assert turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)
    turns.perform_action(players[0], PlayerAction.SHIFT_ACTION)
    turns.perform_action(players[0], PlayerAction.MOVE_ACTION)
    assert not turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)


def test_is_action_possible_with_complete_cycle():
    """ Tests is_action_possible """
    players = [Player(id, 0) for id in [2, 1]]
    turns = Turns(players=players)
    assert turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[0], PlayerAction.MOVE_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.MOVE_ACTION)
    turns.perform_action(players[0], PlayerAction.SHIFT_ACTION)
    turns.perform_action(players[0], PlayerAction.MOVE_ACTION)
    turns.perform_action(players[1], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[0], PlayerAction.MOVE_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)
    assert turns.is_action_possible(players[1], PlayerAction.MOVE_ACTION)
    turns.perform_action(players[1], PlayerAction.MOVE_ACTION)
    assert turns.is_action_possible(players[0], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[0], PlayerAction.MOVE_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.SHIFT_ACTION)
    assert not turns.is_action_possible(players[1], PlayerAction.MOVE_ACTION)
    turns.perform_action(players[0], PlayerAction.SHIFT_ACTION)


def test_start__calls_callback_if_present():
    """ Tests start for player with callback """
    turns = Turns()
    player = Player(7, 0)
    callback = Mock()
    turns.add_player(player, turn_callback=callback)
    callback.assert_not_called()
    turns.start()
    callback.assert_called_once_with(PlayerAction.SHIFT_ACTION)


def test_perform_action__calls_callback_if_present():
    """ Tests perform_action for player with callback """
    turns = Turns()
    player1 = Mock()
    player2 = Mock()
    turns.add_player(player1)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    player2.callback.assert_not_called()
    turns.perform_action(player1, PlayerAction.MOVE_ACTION)
    player2.callback.assert_called_once_with(PlayerAction.SHIFT_ACTION)
    player2.callback.reset_mock()
    turns.perform_action(player2, PlayerAction.SHIFT_ACTION)
    player2.callback.assert_called_once_with(PlayerAction.MOVE_ACTION)


def test_callback_with_one_player():
    """ Tests that the callback on a player is called after each move, even if he plays alone """
    turns = Turns()
    player1 = Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.start()
    player1.callback.assert_called_once_with(PlayerAction.SHIFT_ACTION)
    player1.callback.reset_mock()
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    player1.callback.assert_called_once_with(PlayerAction.MOVE_ACTION)
    player1.callback.reset_mock()
    turns.perform_action(player1, PlayerAction.MOVE_ACTION)
    player1.callback.assert_called_once_with(PlayerAction.SHIFT_ACTION)


def test_removed_player_no_callback_called():
    """ Tests remove_player """
    turns = Turns()
    player1 = Mock()
    player2 = Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    turns.remove_player(player2)
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    turns.perform_action(player1, PlayerAction.MOVE_ACTION)
    player2.callback.assert_not_called()


def test_remove_first_player_next_player_action_should_be_remaining_player():
    """ Tests after removing the first of two players, next_player_action() should not return None """
    turns = Turns()
    player1, player2 = Mock(), Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    turns.remove_player(player1)
    assert turns.next_player_action() is not None
    assert turns.next_player_action().player is player2
    player2.callback.assert_called_once()


def test_remove_second_player_next_player_action_should_be_remaining_player():
    """ Tests after removing the second of two players, next_player_action() should not return None """
    turns = Turns()
    player1, player2 = Mock(), Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    turns.perform_action(player1, PlayerAction.MOVE_ACTION)
    player1.callback.reset_mock()
    turns.remove_player(player2)
    assert turns.next_player_action() is not None
    assert turns.next_player_action().player is player1
    player1.callback.assert_called_once()


def test_remove_player__with_only_one_player__no_callback_called():
    turns = Turns()
    player = Mock()
    turns.add_player(player, turn_callback=player.callback)
    turns.start()
    player.callback.reset_mock()
    turns.remove_player(player)
    assert turns.next_player_action() is None
    player.callback.assert_not_called()


def given_delay__when_start__is_in_prepare_state():
    player = Player(0)
    turns = Turns(prepare_delay=timedelta(milliseconds=10), players=[player])
    turns.start()

    assert turns.next_player_action().action == PlayerAction.PREPARE_SHIFT


def given_prepare_state__when_player_tries_shift__raises_exception():
    player = Player(0)
    turns = Turns(prepare_delay=timedelta(milliseconds=10), players=[player])
    turns.start()

    with pytest.raises(TurnActionViolationException):
        turns.perform_action(player, PlayerAction.SHIFT_ACTION)


def given_delay__when_start__calls_callback_on_player():
    turns = Turns(prepare_delay=timedelta(milliseconds=5))
    player = Mock()
    turns.add_player(player, turn_callback=player.callback)
    turns.start()

    assert player.callback.call_count == 1
    player.callback.assert_called_with(PlayerAction.PREPARE_SHIFT)


def given_delay__when_waiting_long_enough__then_is_in_shift_state():
    player = Player(0)
    turns = Turns(prepare_delay=timedelta(milliseconds=5), players=[player])
    turns.start()

    time.sleep(timedelta(milliseconds=20).total_seconds())

    assert turns.next_player_action().action == PlayerAction.SHIFT_ACTION


def test_given_delay__when_waiting_long_enough__then_calls_callback_on_player():
    turns = Turns(prepare_delay=timedelta(milliseconds=5))
    player = Mock()
    turns.add_player(player, turn_callback=player.callback)
    turns.start()

    time.sleep(timedelta(milliseconds=20).total_seconds())

    assert player.callback.call_count == 2
    player.callback.assert_called_with(PlayerAction.SHIFT_ACTION)


def test_given_delay_with_two_players__when_player1_leaves_during_wait__has_prepare_player2_state():
    turns = Turns(prepare_delay=timedelta(milliseconds=30))
    player1, player2 = Mock(), Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    time.sleep(timedelta(milliseconds=50).total_seconds())
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    time.sleep(timedelta(milliseconds=2).total_seconds())

    turns.remove_player(player1)

    assert player2.callback.call_count == 1
    assert turns.next_player_action().player == player2
    assert turns.next_player_action().action == PlayerAction.PREPARE_SHIFT


def test_given_delay_with_two_players__when_player1_leaves_during_wait__another_wait_leads_to_player2_shift():
    turns = Turns(prepare_delay=timedelta(milliseconds=30))
    player1, player2 = Player(1), Player(2)
    callback1, callback2 = Mock(), Mock()
    turns.add_player(player1, turn_callback=callback1)
    turns.add_player(player2, turn_callback=callback2)
    turns.start()
    time.sleep(timedelta(milliseconds=50).total_seconds())
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    time.sleep(timedelta(milliseconds=5).total_seconds())
    turns.remove_player(player1)
    callback2.reset_mock()

    time.sleep(timedelta(milliseconds=50).total_seconds())

    assert callback2.call_count == 1
    assert turns.next_player_action().player == player2
    assert turns.next_player_action().action == PlayerAction.SHIFT_ACTION


def test_given_delay_with_two_players__when_player1_leaves_during_wait__player1_callback_is_not_called():
    turns = Turns(prepare_delay=timedelta(milliseconds=30))
    player1, player2 = Mock(), Mock()
    turns.add_player(player1, turn_callback=player1.callback)
    turns.add_player(player2, turn_callback=player2.callback)
    turns.start()
    time.sleep(timedelta(milliseconds=50).total_seconds())
    turns.perform_action(player1, PlayerAction.SHIFT_ACTION)
    time.sleep(timedelta(milliseconds=5).total_seconds())
    player1.callback.reset_mock()

    turns.remove_player(player1)
    time.sleep(timedelta(milliseconds=50).total_seconds())

    player1.callback.assert_not_called()
