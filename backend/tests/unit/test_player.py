""" Tests Player of model.py """
from labyrinth.model import Player

def test_identifier():
    """ Tests identifier """
    player = Player(identifier=7)
    assert player.identifier == 7
