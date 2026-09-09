-- NPC behaviour. The server calls into this file for two things:
--
--   npc_stats        once per level while starting up. The result is cached,
--                    so this is never called again while the game runs.
--   npc_decide_move  every time an NPC with a player in view takes a turn.


-- Decides how an NPC moves.
--
--   level   the NPC's level, which its type is derived from
--   returns dx, dy for one step. 0, 0 keeps the NPC where it is.
--
-- The server checks the target tile for the map edge and for walls, so a step
-- into either simply leaves the NPC in place.

function npc_decide_move(level)
	-- Slimes and anything else up to level 10 sit still and wait.
	if level <= 10 then
		return 0, 0
	end

	local dir = math.random(4)
	if dir == 1 then return 0, -1 end	-- up
	if dir == 2 then return 0, 1 end	-- down
	if dir == 3 then return -1, 0 end	-- left
	return 1, 0							-- right
end


-- Stats for an NPC of this level.
--
--   returns max_hp, damage, attack_range, kill_exp
--
-- attack_range is in tiles: the NPC hits a player standing that far away or
-- closer. kill_exp is what a player gains for killing it.

function npc_stats(level)
	local range
	if level <= 10 then range = 3		-- slimes reach the furthest
	elseif level <= 20 then range = 2
	else range = 1 end

	return level * 50, level * 2, range, level * 50
end
