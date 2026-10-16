if ( seterrorhandler ) then
	seterrorhandler(function(message)
		EditorPrint("Lua error: "..tostring(message));
	end);
end

PlaySound = PlaySound or function() end;


GameTooltip = GameTooltip or setmetatable({}, { __index = function() return function() end end });
GameTooltip_AddNewbieTip = GameTooltip_AddNewbieTip or function() end;

format = format or string.format;
floor = floor or math.floor;
mod = mod or math.fmod;
getglobal = getglobal or function(name) return _G[name]; end
max = max or math.max;
min = min or math.min;
abs = abs or math.abs;
tinsert = tinsert or table.insert;
