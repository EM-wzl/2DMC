-- TEMP 流体真机自检（仅 2DMC_AUTOTEST=1 时由 ServerApplication 加载；验证后移除）
-- 注意：引擎 Lua 未开放 io 库；本脚本通过【返回字符串】把结果交给 C++ logInfo 落盘
-- autotest_server_run：在区块 (0,0,0) 搭建 9x9 石板测试台，中心放活跃水源
-- autotest_server_tick：每 20 帧统计测试区内流体方块数；连续 5 次不变即 STABLE

local FLUID_IDS = nil
local STATE = { tick = 0, stable = 0, last = -1, done = false }

local function ensure()
	if FLUID_IDS then return end
	FLUID_IDS = {}
	local R = openminer.registry
	FLUID_IDS[R:get_block_from_string("default:water"):id()] = true
	FLUID_IDS[R:get_block_from_string("default:water_active"):id()] = true
	for lvl = 1, 7 do
		FLUID_IDS[R:get_block_from_string("default:water_flow_" .. lvl):id()] = true
		FLUID_IDS[R:get_block_from_string("default:water_still_" .. lvl):id()] = true
	end
	FLUID_IDS[R:get_block_from_string("default:lava"):id()] = true
	FLUID_IDS[R:get_block_from_string("default:lava_active"):id()] = true
	for lvl = 1, 3 do
		FLUID_IDS[R:get_block_from_string("default:lava_flow_" .. lvl):id()] = true
		FLUID_IDS[R:get_block_from_string("default:lava_still_" .. lvl):id()] = true
	end
end

function autotest_server_run(world)
	ensure()
	local R = openminer.registry
	local stone = R:get_block_from_string("default:stone"):id()
	local water_active = R:get_block_from_string("default:water_active"):id()

	-- 测试台：z=10 铺 9x9 石板（x/y 4..12），上方清空；中心 (8,8,11) 放活跃水源
	for x = 4, 12 do
		for y = 4, 12 do
			world:set_block(x, y, 10, stone)
			for z = 11, 16 do world:set_block(x, y, z, 0) end
		end
	end
	world:set_block(8, 8, 11, water_active)

	STATE.tick = 0
	return "run: water_active(id=" .. water_active .. ") placed at (8,8,11)"
		.. "; readback center=" .. world:get_block(8, 8, 11)
		.. " floor=" .. world:get_block(8, 8, 10)
end

function autotest_server_tick(world)
	if STATE.done then return nil end
	ensure()
	STATE.tick = STATE.tick + 1
	if STATE.tick % 20 ~= 0 then return nil end

	local total = 0
	local cells = {}
	for x = 0, 16 do
		for y = 0, 16 do
			for z = 9, 18 do
				local id = world:get_block(x, y, z)
				if FLUID_IDS[id] then
					total = total + 1
					cells[#cells + 1] = x .. "," .. y .. "," .. z .. "=" .. id
				end
			end
		end
	end

	if total == STATE.last then STATE.stable = STATE.stable + 1 else STATE.stable = 0 end
	STATE.last = total

	local summary = "tick=" .. STATE.tick .. " total=" .. total .. " " .. table.concat(cells, " ")
	if STATE.stable >= 5 then
		STATE.done = true
		return summary .. " | STABLE: " .. total .. " fluid blocks"
	end
	return summary
end
