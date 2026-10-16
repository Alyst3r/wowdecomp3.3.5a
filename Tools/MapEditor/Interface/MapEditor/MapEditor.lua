local ZEROPOINT = 17066.666;
local TILESIZE = 533.33333;
local TOOLBAR_SPACING = 4;
local TOOLBAR_PADDING = 5;

local toolButtons = {};
local toolPanels = {};
local viewButtons = {};
local statusLabels = {};
local activeTool;
local toolPanelShown = true;
local dockFloating = false;
local viewportLevelSet = false;
local FLOATING_DOCK_HEIGHT = 710;
local uiHidden = false;
local screenWidth, screenHeight;
local timeSpeed = 0;
local timeOfDay = 720;
local fpsElapsed = 0;
local fpsFrames = 0;
local fpsText = "FPS: 0 - Average frame time: 0ms";

local VIEW_ITEMS = {
	{ text = "Doodads", icon = 0xF8AB, flag = "doodads" },
	{ text = "WMOs", icon = 0xF8A9, flag = "wmo" },
	{ text = "WMO doodads", icon = 0xF8AA },
	{ text = "WMO exterior", icon = 0xF8C6 },
	{ text = "Terrain", icon = 0xF8AE, flag = "terrain" },
	{ text = "Water", icon = 0xF8B3, flag = "water" },
	{ separator = true },
	{ text = "Lines", icon = 0xF8AF, flag = "lines" },
	{ text = "Hole lines", icon = 0xF8B7, flag = "holelines" },
	{ text = "Wireframe", icon = 0xF8B0, flag = "wireframe" },
	{ text = "Contours", icon = 0xF8B1 },
	{ text = "Climb", icon = 0xF8D2 },
	{ text = "Vertex Color", icon = 0xF8D1 },
	{ text = "Baked Shadows", icon = 0xF8D3 },
	{ separator = true },
	{ text = "Animations", icon = 0xF8B8, flag = "animations" },
	{ text = "Fog", icon = 0xF8B2, flag = "fog" },
	{ text = "Flight bounds", icon = 0xF8DF },
	{ separator = true },
	{ text = "Models with box", icon = 0xF8AC, flag = "modelboxes" },
	{ text = "Hidden models", icon = 0xF8B6 },
	{ separator = true },
	{ text = "Show Minimap", icon = 0xF8A5 },
	{ text = "Details info", icon = 0xF8BB },
};

local HOTKEY_FLAGS = {
	["F1"] = "doodads",
	["F3"] = "terrain",
	["F4"] = "water",
	["F6"] = "wmo",
	["F10"] = "wireframe",
	["F11"] = "animations",
	["F12"] = "fog",
	["F7"] = "lines",
	["SHIFT-F1"] = "holelines",
};

local function ToggleViewFlag(flag)
	EditorSetViewFlag(flag, not EditorGetViewFlag(flag));
	MapEditor_RefreshViewToolbar();
end

local function FlagChecked(flag)
	return function()
		return EditorGetViewFlag(flag);
	end;
end

local function FlagToggle(flag)
	return function()
		ToggleViewFlag(flag);
	end;
end

local function Disabled(text, shortcut)
	return { text = text, shortcut = shortcut, disabled = true };
end

local function Header(text)
	return { text = text, disabled = true };
end

local function BuildMenus()
	local separator = { separator = true };

	local toolsMenu = { title = "Tools" };

	for index, tool in ipairs(NOGGIT_TOOLS) do
		table.insert(toolsMenu, { text = tool.name, disabled = not tool.mode, checked = function() return activeTool == index; end, onClick = function() MapEditor_SetTool(index); end });
	end

	NoggitMenuBar_Build(EditorMenuBar, {
		MapEditor_NoggitMenu(),
		{
			title = "Editor",
			{ text = "Save current tile", shortcut = "Ctrl+Shift+S", onClick = function() MapEditor_Save("current"); end },
			{ text = "Save changed tiles", shortcut = "Ctrl+S", onClick = function() MapEditor_Save("changed"); end },
			{ text = "Save all tiles", shortcut = "Ctrl+Shift+A", onClick = function() MapEditor_Save("all"); end },
			Disabled("Generate new WDL"),
			separator,
			Disabled("Reload tile", "Shift+J"),
			separator,
			Disabled("Force uid check on next opening"),
			separator,
			{ text = "Add bookmark", shortcut = "Ctrl+F5", onClick = function() EditorAddBookmark(); end },
			Disabled("Write coordinates to port.txt and copy to clipboard"),
		},
		{
			title = "Edit",
			Header("Selected object"),
			separator,
			{ text = "Delete", shortcut = "Del", onClick = EditorDeleteSelection },
			{ text = "Reset rotation", shortcut = "Ctrl+R", onClick = function() EditorSetObjectTransform(nil, nil, nil, 0, 0, 0, nil); end },
			{ text = "Set to ground", shortcut = "PgDown", onClick = EditorObjectSnapToGround },
			separator,
			Header("Options"),
			separator,
			Disabled("Locked cursor mode"),
			separator,
			Header("State"),
			separator,
			{ text = "Undo", shortcut = "Ctrl+Z", onClick = EditorUndo },
			{ text = "Redo", shortcut = "Ctrl+Shift+Z", onClick = EditorRedo },
		},
		{
			title = "Assist",
			Header("Current ADT"),
			separator,
			Disabled("Ensure 4 texture layers"),
			{ text = "Clean up", submenu = {
				Disabled("Clear height map"),
				Disabled("Remove texture duplicates"),
				Disabled("Clear textures"),
				Disabled("Clear textures + set base"),
				Disabled("Clear shadows"),
				Disabled("Clear models"),
				Disabled("Clear duplicate models"),
			} },
			{ text = "Export", submenu = {
				Disabled("Export alphamaps"),
				Disabled("Export alphamaps (current texture)"),
				Disabled("Export vertex color map"),
				Disabled("Export heightmap"),
				Disabled("Export normalmap"),
			} },
			{ text = "Import", submenu = {
				Disabled("Import alphamap (file)"),
				Disabled("Import alphamaps"),
				Disabled("Import heightmap (file)"),
				Disabled("Import heightmap"),
				Disabled("Import watermap (file)"),
				Disabled("Import vertex color map (file)"),
				Disabled("Import vertex color map"),
			} },
			separator,
			Header("Loaded ADTs"),
			separator,
			Disabled("Fix terrain gaps between chunks"),
			Disabled("Cleanup empty texture chunks"),
			separator,
			Header("Global"),
			separator,
			Disabled("Convert Map to 8bits alphamaps"),
			Disabled("Convert Map to 4bits alphamaps (old format)"),
			Disabled("Ensure 4 texture layers"),
			{ text = "Export", submenu = {
				Disabled("Export alphamaps"),
				Disabled("Export alphamaps (current texture)"),
				Disabled("Export heightmap"),
				Disabled("Export vertex color map"),
			} },
			{ text = "Import", submenu = {
				Disabled("Import alphamaps"),
				Disabled("Import heightmaps"),
				Disabled("Import vertex color maps"),
			} },
			{ text = "Debug", submenu = {
				Disabled("Load all tiles"),
			} },
		},
		{
			title = "View",
			Header("Drawing"),
			separator,
			{ text = "Doodads", shortcut = "F1", checked = FlagChecked("doodads"), onClick = FlagToggle("doodads") },
			Disabled("WMO doodads", "F2"),
			{ text = "Terrain", shortcut = "F3", checked = FlagChecked("terrain"), onClick = FlagToggle("terrain") },
			{ text = "Water", shortcut = "F4", checked = FlagChecked("water"), onClick = FlagToggle("water") },
			{ text = "WMOs", shortcut = "F6", checked = FlagChecked("wmo"), onClick = FlagToggle("wmo") },
			{ text = "Lines", shortcut = "F7", checked = FlagChecked("lines"), onClick = FlagToggle("lines") },
			Disabled("Contours", "F9"),
			{ text = "Wireframe", shortcut = "F10", checked = FlagChecked("wireframe"), onClick = FlagToggle("wireframe") },
			{ text = "Toggle Animation", shortcut = "F11", checked = FlagChecked("animations"), onClick = FlagToggle("animations") },
			{ text = "Draw fog", shortcut = "F12", checked = FlagChecked("fog"), onClick = FlagToggle("fog") },
			{ text = "Hole lines", shortcut = "Shift+F1", checked = FlagChecked("holelines"), onClick = FlagToggle("holelines") },
			Disabled("Climb", "Shift+F2"),
			Disabled("Vertex Color", "Shift+F3"),
			Disabled("Baked Shadows", "Shift+F4"),
			Disabled("Flight Bounds"),
			{ text = "Models with box", checked = FlagChecked("modelboxes"), onClick = FlagToggle("modelboxes") },
			Disabled("Hidden models"),
			{ text = "Draw Sky", checked = FlagChecked("sky"), onClick = FlagToggle("sky") },
			Disabled("Draw Skybox"),
			{ text = "Debug", submenu = {
				Disabled("Occlusion boxes"),
			} },
			separator,
			Disabled("Show Node Editor", "Shift+N"),
			Disabled("Game View"),
			separator,
			{ text = "Show ADT borders", checked = FlagChecked("adtborders"), onClick = FlagToggle("adtborders") },
			Disabled("Show light zones"),
			separator,
			{ text = "Toggle UI", shortcut = "Tab", onClick = function() MapEditor_SetUIHidden(not uiHidden); end },
			Disabled("Detail infos"),
			{ text = "Increase time speed", shortcut = "N", onClick = function() timeSpeed = timeSpeed + 90; end },
			{ text = "Decrease time speed", shortcut = "B", onClick = function() timeSpeed = max(0, timeSpeed - 90); end },
			{ text = "Pause time", shortcut = "J", onClick = function() timeSpeed = 0; end },
			Disabled("Invert mouse", "I"),
			{ text = "Decrease camera speed", shortcut = "O", onClick = function() MapEditor_ScaleCameraSpeed(0.5); end },
			{ text = "Increase camera speed", shortcut = "P", onClick = function() MapEditor_ScaleCameraSpeed(2); end },
			separator,
			Disabled("Debug cam"),
			Disabled("FPS camera"),
			Disabled("Camera Collision"),
		},
		toolsMenu,
		{
			title = "Help",
			Disabled("Key Bindings", "Ctrl+F1"),
			Disabled("WoW Modding Discord"),
			Disabled("Noggit Red Repository"),
		},
	});
end

function MapEditor_ScaleCameraSpeed(factor)
	local _, _, _, _, _, speed = EditorGetCamera();
	EditorSetCameraSpeed((speed or 40) * factor);
end

local function BuildLeftToolbar()
	local y = -TOOLBAR_PADDING;

	for index, tool in ipairs(NOGGIT_TOOLS) do
		local button = NoggitToolButton_Create(EditorLeftToolbar, tool.icon, tool.mode and tool.name or (tool.name.." (not available yet)"), function()
			MapEditor_SetTool(index);
		end);
		button:SetPoint("TOP", EditorLeftToolbar, "TOP", 0, y);

		if ( not tool.mode ) then
			button:Disable();
		end

		toolButtons[index] = button;
		y = y - button:GetHeight() - TOOLBAR_SPACING;
	end

	EditorLeftToolbar:SetHeight(-y + TOOLBAR_PADDING - TOOLBAR_SPACING);
	NoggitBox_Set(EditorLeftToolbar, NOGGIT_COLOR_BASE, nil);
end

local function BuildViewToolbar()
	local x = TOOLBAR_PADDING;

	for _, item in ipairs(VIEW_ITEMS) do
		if ( item.separator ) then
			local line = NoggitUI_SolidTexture(EditorViewToolbar, "ARTWORK", NOGGIT_COLOR_MID);
			line:SetWidth(2);
			line:SetPoint("TOPLEFT", EditorViewToolbar, "TOPLEFT", x, -6);
			line:SetPoint("BOTTOMLEFT", EditorViewToolbar, "BOTTOMLEFT", x, 6);
			x = x + 2 + TOOLBAR_SPACING;
		else
			local flag = item.flag;
			local button = NoggitToolButton_Create(EditorViewToolbar, item.icon, flag and item.text or (item.text.." (not available yet)"), function()
				ToggleViewFlag(flag);
			end);
			button:SetPoint("LEFT", EditorViewToolbar, "LEFT", x, 0);
			button.flag = flag;

			if ( not flag ) then
				button:Disable();
			end

			table.insert(viewButtons, button);
			x = x + button:GetWidth() + TOOLBAR_SPACING;
		end
	end

	EditorViewToolbar:SetWidth(x - TOOLBAR_SPACING + TOOLBAR_PADDING);
	NoggitBox_Set(EditorViewToolbar, NOGGIT_COLOR_BASE, nil);
end

function MapEditor_RefreshViewToolbar()
	for _, button in ipairs(viewButtons) do
		if ( button.flag ) then
			NoggitToolButton_SetChecked(button, EditorGetViewFlag(button.flag));
		end
	end
end

local function BuildStatusBar()
	local previous;

	for i = 1, 5 do
		local holder = CreateFrame("Frame", NoggitUI_NewName("NoggitStatusItem"), EditorStatusBar);
		holder:SetHeight(18);

		if ( previous ) then
			holder:SetPoint("LEFT", previous, "RIGHT", 4, 0);
		else
			holder:SetPoint("LEFT", EditorStatusBar, "LEFT", 4, 0);
		end

		local border = NoggitUI_SolidTexture(holder, "BORDER", NOGGIT_COLOR_MID);
		border:SetWidth(1);
		border:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, 0);
		border:SetPoint("BOTTOMLEFT", holder, "BOTTOMLEFT", 0, 0);

		local text = NoggitUI_FontString(holder, "");
		text:SetPoint("LEFT", holder, "LEFT", 4, 0);
		holder.text = text;
		statusLabels[i] = holder;
		previous = holder;
	end
end

local function SetStatus(index, text)
	local holder = statusLabels[index];
	holder.text:SetText(text);
	holder:SetWidth(holder.text:GetStringWidth() + 8);
end

local function UpdateStatus(elapsed)
	local x, y, z = EditorGetCamera();

	if ( not x ) then
		return;
	end

	local clientX = ZEROPOINT - y;
	local clientZ = ZEROPOINT - x;
	SetStatus(1, format("tile: %d %d; coordinates client: (%.2f, %.2f, %.2f), server: (%.2f, %.2f, %.2f)", floor(clientX / TILESIZE), floor(clientZ / TILESIZE), clientX, clientZ, z, x, y, z));
	SetStatus(2, EditorGetAreaName(x, y));

	local minutes = floor(timeOfDay);
	SetStatus(3, format("Time: %d:%02d, Pres: 0", floor(minutes / 60), mod(minutes, 60)));

	fpsElapsed = fpsElapsed + elapsed;
	fpsFrames = fpsFrames + 1;

	if ( fpsElapsed > 1 ) then
		local average = fpsElapsed / fpsFrames;
		fpsText = format("FPS: %d - Average frame time: %sms", floor(1 / average), tostring(floor(average * 1000000 + 0.5) / 1000));
		fpsElapsed = 0;
		fpsFrames = 0;
	end

	SetStatus(4, fpsText);

	local _, chunks, areas, _, _, _, objects, renderedObjects = EditorGetStats();
	SetStatus(5, format("Loaded tiles: %d, Rendered chunks: %d     Loaded objects: %d, Rendered objects: %d", areas, chunks, objects, renderedObjects));
end

local function LayoutViewport()
	EditorViewportFrame:ClearAllPoints();
	EditorViewportFrame:SetPoint("TOPLEFT", EditorMenuBar, "BOTTOMLEFT", 0, 0);

	if ( toolPanelShown and not uiHidden and not dockFloating ) then
		EditorViewportFrame:SetPoint("BOTTOMRIGHT", EditorToolDock, "BOTTOMLEFT", 0, 0);
	else
		EditorViewportFrame:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", 0, 0);
	end
end

local function DockToolPanel()
	EditorToolDock:ClearAllPoints();
	EditorToolDock:SetWidth(265);
	EditorToolDock:SetPoint("TOPRIGHT", EditorMenuBar, "BOTTOMRIGHT", 0, 0);
	EditorToolDock:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", 0, 0);
end

local function FloatToolPanel()
	EditorToolDock:ClearAllPoints();
	EditorToolDock:SetWidth(265);
	local panel = activeTool and toolPanels[activeTool];
	EditorToolDock:SetHeight(min(panel and (panel:GetHeight() + 33) or FLOATING_DOCK_HEIGHT, screenHeight and (screenHeight - 80) or FLOATING_DOCK_HEIGHT));
	EditorToolDock:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", -20, 20);
end

function MapEditor_SetDockFloating(floating)
	if ( floating == dockFloating ) then
		return;
	end

	dockFloating = floating;

	if ( floating ) then
		FloatToolPanel();
		toolPanelShown = true;
		EditorToolDock:Show();

		if ( not EditorPopoutFrame("EditorToolDock", EditorToolDockTitleText:GetText() or "Tool") ) then
			dockFloating = false;
			DockToolPanel();
		end
	else
		EditorDockFrame("EditorToolDock");
		DockToolPanel();
	end

	LayoutViewport();
end

function MapEditor_ToggleDockFloating()
	MapEditor_SetDockFloating(not dockFloating);
end

local floatingWindows = {};

local function ShowFloatingWindow(frame, title, x, y)
	if ( frame.floating ) then
		return;
	end

	frame:ClearAllPoints();
	frame:SetPoint("TOPLEFT", EditorMenuBar, "BOTTOMLEFT", x, y);
	frame:Show();

	if ( EditorPopoutFrame(frame:GetName(), title) ) then
		frame.floating = true;
		floatingWindows[frame:GetName()] = frame;
		EditorSetPopoutVisible(frame:GetName(), not uiHidden);
	else
		frame:Hide();
	end
end

local function HideFloatingWindow(frame)
	if ( not frame.floating ) then
		return;
	end

	EditorDockFrame(frame:GetName());
	frame.floating = false;
	floatingWindows[frame:GetName()] = nil;
	frame:Hide();
end

function MapEditor_ShowTexturePalette()
	MapEditor_RefreshTexturePalette();
	ShowFloatingWindow(EditorTexturePalette, "Texture Palette", 70, -60);
end

function MapEditor_ShowAssetBrowser()
	MapEditor_RefreshAssetBrowser();
	ShowFloatingWindow(EditorAssetBrowser, "Asset browser", 300, -60);
end

function MapEditor_ShowRotationEditor()
	ShowFloatingWindow(EditorRotationEditor, "Pos/Rotation Editor", 460, -60);
end

function MapEditor_OnPopoutClosed(name)
	if ( name == "EditorToolDock" ) then
		MapEditor_SetDockFloating(false);
	elseif ( floatingWindows[name] ) then
		HideFloatingWindow(floatingWindows[name]);
	end
end

function MapEditor_OnPopoutResized(name, width, height)
	local frame = _G[name];

	if ( frame ) then
		frame:SetWidth(width);
		frame:SetHeight(height);
	end
end

function MapEditor_SetEditorShown(shown)
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	if ( shown ) then
		if ( EditorViewportFrame ) then
			EditorViewportFrame:Show();
		end

		return;
	end

	for _, frame in pairs(floatingWindows) do
		HideFloatingWindow(frame);
	end

	MapEditor_SetDockFloating(false);

	if ( EditorViewportFrame ) then
		EditorViewportFrame:Hide();
	end
end

function MapEditor_SetToolPanelShown(shown)
	toolPanelShown = shown;

	if ( not shown and dockFloating ) then
		MapEditor_SetDockFloating(false);
	end

	if ( shown and (dockFloating or not uiHidden) ) then
		EditorToolDock:Show();
	else
		EditorToolDock:Hide();
	end

	if ( dockFloating ) then
		EditorSetPopoutVisible("EditorToolDock", not uiHidden);
	end

	LayoutViewport();
end

function MapEditor_SetUIHidden(hidden)
	uiHidden = hidden;

	if ( hidden ) then
		EditorLeftToolbar:Hide();
		EditorViewToolbar:Hide();
	else
		EditorLeftToolbar:Show();
		EditorViewToolbar:Show();
	end

	for name in pairs(floatingWindows) do
		EditorSetPopoutVisible(name, not hidden);
	end

	MapEditor_SetToolPanelShown(toolPanelShown);
end

function MapEditor_SetTool(index)
	local tool = NOGGIT_TOOLS[index];

	if ( not tool or not tool.mode ) then
		return;
	end

	activeTool = index;
	EditorSetEditingMode(tool.mode);

	for i, button in ipairs(toolButtons) do
		NoggitToolButton_SetChecked(button, i == index);
	end

	for i, panel in pairs(toolPanels) do
		if ( i == index ) then
			panel:Show();
		else
			panel:Hide();
		end
	end

	EditorToolDockTitleText:SetText(tool.name);
	EditorSetPopoutTitle("EditorToolDock", tool.name);

	if ( tool.noPanel ) then
		if ( toolPanelShown ) then
			MapEditor_SetToolPanelShown(false);
		end
	elseif ( not toolPanelShown ) then
		MapEditor_SetToolPanelShown(true);
	end
end

function MapEditor_Save(mode)
	local saved, err = EditorSaveTiles(mode);

	if ( not saved ) then
		NoggitMessageBox_Critical(err);
	else
		EditorPrint(format("MapEditor: saved %d tile(s)", saved));
	end
end

local CTRL_HOTKEYS = {
	["CTRL-S"] = function() MapEditor_Save("changed"); end,
	["CTRL-SHIFT-S"] = function() MapEditor_Save("current"); end,
	["CTRL-SHIFT-A"] = function() MapEditor_Save("all"); end,
	["CTRL-F5"] = function() EditorAddBookmark(); end,
	["CTRL-R"] = function() EditorSetObjectTransform(nil, nil, nil, 0, 0, 0, nil); end,
	["CTRL-C"] = function() if ( EditorGetEditingMode() == 4 ) then EditorCopySelection(); end end,
	["CTRL-V"] = function() if ( EditorGetEditingMode() == 4 ) then EditorPasteObjects(true); end end,
	["CTRL-B"] = function() if ( EditorGetEditingMode() == 4 ) then EditorDuplicateSelection(); end end,
	["T"] = function() if ( EditorGetEditingMode() == 5 ) then EditorSetTileHoles(false); end end,
	["ALT-T"] = function() if ( EditorGetEditingMode() == 5 ) then EditorSetTileHoles(true); end end,
	["F"] = function() if ( EditorGetEditingMode() == 6 ) then EditorSetTileArea(); end end,
};

function MapEditor_OnHotkey(key)
	if ( CTRL_HOTKEYS[key] ) then
		CTRL_HOTKEYS[key]();
		return;
	end

	local flag = HOTKEY_FLAGS[key];

	if ( flag ) then
		ToggleViewFlag(flag);
	elseif ( key == "TAB" ) then
		MapEditor_SetUIHidden(not uiHidden);
	elseif ( key == "N" ) then
		timeSpeed = timeSpeed + 90;
	elseif ( key == "B" ) then
		timeSpeed = max(0, timeSpeed - 90);
	elseif ( key == "J" ) then
		timeSpeed = 0;
	elseif ( key == "O" ) then
		MapEditor_ScaleCameraSpeed(0.5);
	elseif ( key == "P" ) then
		MapEditor_ScaleCameraSpeed(2);
	end
end

local function UpdateScale()
	local width, height = EditorGetWindowSize();

	if ( width == screenWidth and height == screenHeight ) then
		return;
	end

	screenWidth = width;
	screenHeight = height;

	if ( height > 0 ) then
		EditorParent:SetScale(768 / height);
	end
end

function MapEditor_OnLoad(self)
	UpdateScale();

	NoggitBox_Set(EditorMenuBar, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorStatusBar, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorToolDock, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorToolDockTitle, NOGGIT_COLOR_DARK, nil);
	NoggitBox_Set(EditorToolDockClose, NOGGIT_COLOR_DARK, nil);
	NoggitBox_Set(EditorToolDockFloat, NOGGIT_COLOR_DARK, nil);

	BuildMenus();
	BuildLeftToolbar();
	BuildViewToolbar();
	BuildStatusBar();

	toolPanels[1] = NoggitTools_CreateRaiseLower(EditorToolDockContent);
	toolPanels[2] = NoggitTools_CreateFlattenBlur(EditorToolDockContent);
	toolPanels[3] = NoggitTools_CreateTexturePainter(EditorToolDockContent);
	toolPanels[4] = NoggitTools_CreateHoleCutter(EditorToolDockContent);
	toolPanels[5] = NoggitTools_CreateAreaDesignator(EditorToolDockContent);
	toolPanels[8] = NoggitTools_CreateVertexPainter(EditorToolDockContent);
	toolPanels[9] = NoggitTools_CreateObjectEditor(EditorToolDockContent);
	MapEditor_CreateTexturePalette();
	MapEditor_CreateRotationEditor();
	MapEditor_CreateAssetBrowser();

	timeOfDay = EditorGetTimeOfDay();
	MapEditor_SetTool(EditorGetEditingMode());
	MapEditor_RefreshViewToolbar();

	MapEditor_CreateStartScreens();

	if ( EditorIsMapLoaded() ) then
		local directory, mapId = EditorGetMapInfo();
		EditorPrint(format("MapEditor: UI ready, map %s (%d)", directory, mapId));
	else
		EditorPrint("MapEditor: UI ready, no map");
		MapEditor_StartFlow();
	end
end

function MapEditor_OnUpdate(self, elapsed)
	UpdateScale();

	if ( not viewportLevelSet and EditorViewportFrame ) then
		viewportLevelSet = true;
		EditorViewportFrame:SetFrameLevel(127);
	end
	NoggitMenu_OnUpdate(elapsed);

	local editorShown = EditorIsMapLoaded() and not EditorStartScreen:IsShown();

	if ( EditorViewportFrame and editorShown and not EditorViewportFrame:IsShown() ) then
		EditorViewportFrame:Show();
	elseif ( EditorViewportFrame and not editorShown and EditorViewportFrame:IsShown() ) then
		EditorViewportFrame:Hide();
	end

	if ( not editorShown ) then
		return;
	end

	local mode = EditorGetEditingMode();

	if ( not activeTool or NOGGIT_TOOLS[activeTool].mode ~= mode ) then
		for index, tool in ipairs(NOGGIT_TOOLS) do
			if ( tool.mode == mode ) then
				MapEditor_SetTool(index);
			end
		end
	end

	if ( activeTool and toolPanels[activeTool] ) then
		NoggitTools_Sync(toolPanels[activeTool]);
	end

	if ( timeSpeed > 0 ) then
		timeOfDay = mod(timeOfDay + elapsed * timeSpeed / 2, 1440);
		EditorSetTimeOfDay(timeOfDay);
	end

	UpdateStatus(elapsed);
end
