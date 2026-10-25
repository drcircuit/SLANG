lib players:
    public obj Player:
        name as text
        health as int
    .

    public func Create <name as text>:
        ret Player<name, 100>
    .

    public func Heal <player as Player, amount as int>:
        assert<amount >= 0>
        player.health = player.health + amount
    .
.
