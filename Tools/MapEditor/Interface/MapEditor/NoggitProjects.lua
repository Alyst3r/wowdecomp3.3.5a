local TEXTURE_PATH = "Interface\\MapEditor\\Textures\\";
local GLYPH_STAR = 0xF005;
local GLYPH_COG = 0xF013;
local GLYPH_PLUS = 0xF067;
local GLYPH_BOOKMARK = 0xF02E;
local GLYPH_CLOUD = 0xF0C2;
local GLYPH_FOLDER_OPEN = 0xF07C;
local GLYPH_GAMEPAD = 0xF11B;

local EXPANSION_ICONS = { "classic", "burning", "wrath", "cata", "panda", "warlords", "legion", "battle", "shadow" };
local INSTANCE_TYPES = { [0] = "Continent", [1] = "Dungeon", [2] = "Raid", [3] = "Battleground", [4] = "Arena", [5] = "Scenario" };
local VERSION_ICONS = { ["Wrath Of The Lich King"] = "wrath", ["Shadowlands"] = "shadow" };

local startScreen;
local projectWindow;
local projectList;
local creationDialog;
local mapScreen;
local mapList;
local bookmarkList;
local minimap;
local contextTarget;
local currentMinimapMap;
local allMaps = {};
local searchName;
local searchType;
local searchExpansion;
local searchWmo;

local function ExpansionIcon(expansion)
	local name = EXPANSION_ICONS[(expansion or 0) + 1];
	return name and (TEXTURE_PATH.."icon_"..name..".tga") or nil;
end

local function InfoText(parent, text, justify)
	local fontString = NoggitUI_FontString(parent, text, "NoggitFont10", justify);
	NoggitText_SetColor(fontString, NOGGIT_COLOR_INFO);
	return fontString;
end

local function Glyph(parent, codepoint, font, color)
	local fontString = parent:CreateFontString(nil, "ARTWORK", font or "NoggitAwesomeFont16");
	fontString:SetText(NoggitUI_Utf8(codepoint));

	if ( color ) then
		NoggitText_SetColor(fontString, color);
	end

	return fontString;
end

local function BigButton(parent, text, width, onClick)
	local button = NoggitPushButton_Create(parent, text, width, onClick);
	button:SetHeight(40);
	button:SetNormalFontObject(NoggitFont16);
	button:SetHighlightFontObject(NoggitFont16);
	button:SetDisabledFontObject(NoggitFont16Disabled);
	button:GetFontString():SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	return button;
end

local function ShowStartScreen(content)
	startScreen:Show();

	if ( content == projectWindow ) then
		projectWindow:Show();
		mapScreen:Hide();
		NoggitBox_Set(startScreen, { 0.08, 0.08, 0.09, 1 }, nil);
	else
		projectWindow:Hide();
		mapScreen:Show();
		NoggitBox_Set(startScreen, NOGGIT_COLOR_BASE, nil);
	end

	creationDialog:Hide();
	MapEditor_SetEditorShown(false);
end

local function RefreshProjects()
	NoggitListBox_SetItems(projectList, EditorGetRecentProjects());
end

local function OpenProject(path)
	local ok, message = EditorOpenProject(path);

	if ( not ok ) then
		NoggitMessageBox_Critical(message or "Failed to read project");
		RefreshProjects();
		return;
	end

	if ( message == "relaunch" ) then
		return;
	end

	MapEditor_ShowMapSelection();
end

local PROJECT_MENU = {
	{ text = "Forget Project", icon = GLYPH_CLOUD, onClick = function()
		local path = contextTarget.path;
		NoggitMessageBox_Show("Forget Project", "Data on the disk will not be removed, this action will only hide the project. Continue?.", {
			{ text = "Accept", onClick = function()
				EditorForgetProject(path);
				RefreshProjects();
			end },
			{ text = "Cancel" },
		});
	end },
	{ text = "Open Project Directory", icon = GLYPH_FOLDER_OPEN, onClick = function() EditorOpenDirectory(contextTarget.path); end },
	{ text = "Open WoW Client Directory", icon = GLYPH_GAMEPAD, onClick = function() EditorOpenDirectory(contextTarget.client); end },
	{ text = "Favorite Project(auto load)", icon = GLYPH_STAR, onClick = function()
		EditorSetFavoriteProject(contextTarget.path);
		RefreshProjects();
	end },
};

local PROJECT_MENU_FAVORITE = {
	PROJECT_MENU[1],
	PROJECT_MENU[2],
	PROJECT_MENU[3],
	{ text = "Unfavorite Project", icon = GLYPH_STAR, onClick = function()
		EditorSetFavoriteProject(nil);
		RefreshProjects();
	end },
};

local function BuildProjectRow(row)
	row.icon = row:CreateTexture(nil, "ARTWORK");
	row.icon:SetWidth(48);
	row.icon:SetHeight(48);
	row.icon:SetPoint("TOPLEFT", row, "TOPLEFT", 0, -4);

	row.name = NoggitUI_FontString(row, "", "NoggitFont15");
	row.name:SetPoint("TOPLEFT", row, "TOPLEFT", 52, -4);

	row.directory = InfoText(row, "");
	row.directory:SetPoint("TOPLEFT", row, "TOPLEFT", 54, -23);
	row.directory:SetPoint("RIGHT", row, "RIGHT", -8, 0);
	row.directory:SetHeight(12);

	row.version = InfoText(row, "");
	row.version:SetPoint("TOPLEFT", row, "TOPLEFT", 54, -37);

	row.date = InfoText(row, "", "RIGHT");
	row.date:SetPoint("TOPRIGHT", row, "TOPRIGHT", -8, -37);

	row.star = Glyph(row, GLYPH_STAR, "NoggitAwesomeFont16", NOGGIT_COLOR_GOLD);
	row.star:SetPoint("TOPRIGHT", row, "TOPRIGHT", -8, -8);
end

local function RefreshProjectRow(row, project)
	row.icon:SetTexture(TEXTURE_PATH.."icon_"..(VERSION_ICONS[project.version] or "wrath")..".tga");
	row.name:SetText(NoggitUI_CamelCase(project.name));
	row.directory:SetText(project.path);
	row.version:SetText(project.version);
	row.date:SetText(EditorGetDate());

	if ( project.favorite ) then
		row.star:Show();
	else
		row.star:Hide();
	end
end

local function CreateProjectWindow()
	projectWindow = CreateFrame("Frame", "EditorProjectWindow", startScreen, "NoggitBoxTemplate");
	projectWindow:SetWidth(792);
	projectWindow:SetHeight(416 + 30);
	projectWindow:SetPoint("CENTER", startScreen, "CENTER", 0, 0);
	projectWindow:EnableMouse(true);
	NoggitBox_Set(projectWindow, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);

	local titleBar = CreateFrame("Frame", "EditorProjectWindowTitle", projectWindow, "NoggitBoxTemplate");
	titleBar:SetHeight(30);
	titleBar:SetPoint("TOPLEFT", projectWindow, "TOPLEFT", 1, -1);
	titleBar:SetPoint("TOPRIGHT", projectWindow, "TOPRIGHT", -1, -1);
	NoggitBox_Set(titleBar, NOGGIT_COLOR_DARK, nil);

	local titleIcon = titleBar:CreateTexture(nil, "ARTWORK");
	titleIcon:SetTexture(TEXTURE_PATH.."icon_wrath.tga");
	titleIcon:SetWidth(18);
	titleIcon:SetHeight(18);
	titleIcon:SetPoint("LEFT", titleBar, "LEFT", 8, 0);

	local title = NoggitUI_FontString(titleBar, "Noggit Red Project Selection");
	title:SetPoint("LEFT", titleIcon, "RIGHT", 8, 0);

	local close = CreateFrame("Button", "EditorProjectWindowClose", titleBar, "NoggitBoxTemplate");
	close:SetWidth(40);
	close:SetHeight(30);
	close:SetPoint("RIGHT", titleBar, "RIGHT", 0, 0);
	local closeIcon = close:CreateTexture(nil, "ARTWORK");
	closeIcon:SetTexture(TEXTURE_PATH.."icon_close.tga");
	closeIcon:SetWidth(14);
	closeIcon:SetHeight(14);
	closeIcon:SetTexCoord(0, 0.875, 0, 0.875);
	closeIcon:SetPoint("CENTER", close, "CENTER", 0, 0);
	close:SetScript("OnEnter", function(self) NoggitBox_Set(self, { 0.78, 0.18, 0.18, 1 }, nil); end);
	close:SetScript("OnLeave", function(self) NoggitBox_Set(self, nil, nil); end);
	close:SetScript("OnClick", function() EditorQuit(); end);

	local content = CreateFrame("Frame", "EditorProjectWindowContent", projectWindow);
	content:SetPoint("TOPLEFT", titleBar, "BOTTOMLEFT", 0, 0);
	content:SetPoint("BOTTOMRIGHT", projectWindow, "BOTTOMRIGHT", -1, 1);

	local recentLabel = NoggitUI_FontString(content, "Recent Projects", "NoggitFont18");
	recentLabel:SetPoint("TOPLEFT", content, "TOPLEFT", 14, -12);

	projectList = NoggitListBox_Create(content, 55, BuildProjectRow, RefreshProjectRow);
	projectList:SetWidth(380);
	projectList:SetPoint("TOPLEFT", content, "TOPLEFT", 14, -42);
	projectList:SetPoint("BOTTOM", content, "BOTTOM", 0, 14);
	projectList.onDoubleClick = function(project)
		OpenProject(project.path);
	end;
	projectList.onContext = function(project)
		contextTarget = project;

		if ( project.favorite ) then
			NoggitContextMenu_Show("projectFavorite", PROJECT_MENU_FAVORITE);
		else
			NoggitContextMenu_Show("project", PROJECT_MENU);
		end
	end;

	local line = NoggitSeparator_Create(content, true);
	line:SetPoint("TOPLEFT", content, "TOPLEFT", 408, -14);
	line:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 408, 14);

	local startedLabel = NoggitUI_FontString(content, "Getting Started", "NoggitFont18");
	startedLabel:SetPoint("TOPLEFT", content, "TOPLEFT", 422, -12);

	local width = 792 - 422 - 14;
	local create = BigButton(content, "Create a new project", width, function()
		MapEditor_ShowCreationDialog();
	end);
	create:SetPoint("TOPLEFT", content, "TOPLEFT", 422, -48);

	local open = BigButton(content, "Open an existing project", width, function()
		local file, directory = EditorBrowseProjectFile();

		if ( not file ) then
			NoggitMessageBox_Critical("Failed to read project: project file is empty");
			return;
		end

		OpenProject(directory);
	end);
	open:SetPoint("TOPLEFT", create, "BOTTOMLEFT", 0, -6);

	local convert = BigButton(content, "Convert Project", width);
	convert:SetPoint("TOPLEFT", open, "BOTTOMLEFT", 0, -6);
	convert:Disable();

	local settings = CreateFrame("Button", "EditorProjectSettingsButton", content, "NoggitToolButtonTemplate");
	settings:SetWidth(34);
	settings:SetHeight(30);
	settings:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -14, 14);
	local settingsIcon = _G[settings:GetName().."Icon"];
	settingsIcon:SetFontObject(NoggitAwesomeFont16);
	settingsIcon:SetText(NoggitUI_Utf8(GLYPH_COG));
	settings.tooltip = "Settings (not available yet)";
	settings:Disable();
	settingsIcon:SetFontObject(NoggitAwesomeFont16);
	NoggitText_SetColor(settingsIcon, NOGGIT_COLOR_DISABLED);
end

local function FormRow(parent, y, label)
	local text = NoggitUI_FontString(parent, label);
	text:SetPoint("TOPLEFT", parent, "TOPLEFT", 14, y - 8);
	return text;
end

local function PathField(parent, y, title, getInitial)
	local edit = NoggitLineEdit_Create(parent, 250);
	edit:SetHeight(32);
	edit:SetPoint("TOPLEFT", parent, "TOPLEFT", 112, y);

	local browse = NoggitPushButton_Create(parent, "...", 28, function()
		local initial = getInitial and getInitial(edit) or edit:GetText();
		local path = EditorBrowseFolder(title, initial);

		if ( getInitial ) then
			edit:SetText(initial or "");
		end

		edit:SetText(path or "");
	end);
	browse:SetHeight(24);
	browse:SetPoint("LEFT", edit, "RIGHT", 4, 0);
	return edit;
end

local function CreateCreationDialog()
	creationDialog = CreateFrame("Frame", "EditorProjectCreationDialog", startScreen, "NoggitBoxTemplate");
	creationDialog:SetWidth(448);
	creationDialog:SetHeight(235);
	creationDialog:SetPoint("CENTER", startScreen, "CENTER", 0, 0);
	creationDialog:SetFrameStrata("DIALOG");
	creationDialog:EnableMouse(true);
	creationDialog:Hide();
	NoggitBox_Set(creationDialog, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);

	local blocker = CreateFrame("Frame", "EditorProjectCreationBlocker", creationDialog);
	blocker:SetAllPoints(startScreen);
	blocker:SetFrameLevel(max(1, creationDialog:GetFrameLevel() - 1));
	blocker:EnableMouse(true);
	NoggitUI_SolidTexture(blocker, "BACKGROUND", { 0, 0, 0, 0.35 }):SetAllPoints(blocker);

	local group = CreateFrame("Frame", "EditorProjectCreationGroup", creationDialog, "NoggitGroupBoxTemplate");
	group:SetPoint("TOPLEFT", creationDialog, "TOPLEFT", 9, -9);
	group:SetPoint("TOPRIGHT", creationDialog, "TOPRIGHT", -9, -9);
	group:SetHeight(178);
	local groupTitle = _G[group:GetName().."Title"];
	groupTitle:SetText("New Project Settings");
	_G[group:GetName().."TitleLine"]:SetWidth(groupTitle:GetStringWidth());

	local y = -26;
	FormRow(group, y, "Project Name");
	local name = NoggitLineEdit_Create(group, 250);
	name:SetHeight(32);
	name:SetPoint("TOPLEFT", group, "TOPLEFT", 112, y);

	y = y - 36;
	FormRow(group, y, "Project Path");
	local projectPath = PathField(group, y, "Select Project Directory");

	y = y - 36;
	FormRow(group, y, "Client Path");
	local clientPath = PathField(group, y, "Select Client Directory", function()
		return EditorGetGamePath();
	end);

	y = y - 36;
	FormRow(group, y, "Expansions");
	local expansionIcon = group:CreateTexture(nil, "ARTWORK");
	expansionIcon:SetWidth(32);
	expansionIcon:SetHeight(32);
	expansionIcon:SetTexture(TEXTURE_PATH.."icon_wrath.tga");
	local expansion = NoggitComboBox_Create(group, 250, { { text = "Wrath Of The Lich King" } }, 1, function(_, _, item)
		expansionIcon:SetTexture(TEXTURE_PATH.."icon_"..(VERSION_ICONS[item.text] or "wrath")..".tga");
	end);
	expansion:SetPoint("TOPLEFT", group, "TOPLEFT", 112, y - 4);
	expansionIcon:SetPoint("LEFT", expansion, "RIGHT", 4, 0);

	local ok = NoggitPushButton_Create(creationDialog, "Ok", 80, function()
		local success, message = EditorCreateProject(name:GetText(), projectPath:GetText(), clientPath:GetText(), expansion.items[expansion.selected].text);

		if ( not success ) then
			NoggitMessageBox_Critical(message);
			return;
		end

		creationDialog:Hide();
		RefreshProjects();
	end);
	ok:SetPoint("BOTTOMRIGHT", creationDialog, "BOTTOMRIGHT", -9, 9);

	local cancel = NoggitPushButton_Create(creationDialog, "Cancel", 80, function()
		creationDialog:Hide();
	end);
	cancel:SetPoint("RIGHT", ok, "LEFT", -6, 0);

	creationDialog.fields = { name, projectPath, clientPath };
end

function MapEditor_ShowCreationDialog()
	for _, field in ipairs(creationDialog.fields) do
		field:SetText("");
		field:ClearFocus();
	end

	creationDialog:Show();
end

local MAP_MENU = {
	{ text = "Pin Map", icon = GLYPH_STAR, onClick = function()
		EditorPinMap(contextTarget.id, contextTarget.name, true);
		MapEditor_RefreshMapList();
	end },
};

local MAP_MENU_PINNED = {
	{ text = "Unpin Map", icon = GLYPH_STAR, onClick = function()
		EditorPinMap(contextTarget.id, contextTarget.name, false);
		MapEditor_RefreshMapList();
	end },
};

local function BuildMapRow(row)
	row.icon = row:CreateTexture(nil, "ARTWORK");
	row.icon:SetWidth(32);
	row.icon:SetHeight(32);
	row.icon:SetPoint("LEFT", row, "LEFT", 0, 0);

	row.name = NoggitUI_FontString(row, "");
	row.name:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -2);

	row.id = InfoText(row, "");
	row.id:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -18);

	row.instanceType = InfoText(row, "", "RIGHT");
	row.instanceType:SetPoint("TOPRIGHT", row, "TOPRIGHT", -8, -18);

	row.star = Glyph(row, GLYPH_STAR, "NoggitAwesomeFont", NOGGIT_COLOR_GOLD);
	row.star:SetPoint("TOPRIGHT", row, "TOPRIGHT", -8, -3);
end

local function RefreshMapRow(row, map)
	local icon = ExpansionIcon(map.expansion);

	if ( icon ) then
		row.icon:SetTexture(icon);
		row.icon:Show();
	else
		row.icon:Hide();
	end

	row.name:SetText(NoggitUI_CamelCase(map.name));
	row.id:SetText(tostring(map.id));
	row.instanceType:SetText(INSTANCE_TYPES[map.type] or "Unknown");

	if ( map.pinned ) then
		row.star:Show();
	else
		row.star:Hide();
	end
end

local function BuildBookmarkRow(row)
	row.icon = Glyph(row, GLYPH_BOOKMARK, "NoggitAwesomeFont28", NOGGIT_COLOR_GOLD);
	row.icon:SetWidth(32);
	row.icon:SetPoint("LEFT", row, "LEFT", 0, 0);

	row.name = NoggitUI_FontString(row, "");
	row.name:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -2);

	row.position = InfoText(row, "");
	row.position:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -18);
end

local function RefreshBookmarkRow(row, bookmark)
	row.name:SetText(NoggitUI_CamelCase(bookmark.name));
	row.position:SetText(format("%d , %d , %d", bookmark.x, bookmark.y, bookmark.z));
end

local function FilterMaps()
	local filtered = {};
	local name = string.lower(searchName and searchName:GetText() or "");
	local typeIndex = searchType and (searchType.selected - 1) or 0;
	local expansionIndex = searchExpansion and (searchExpansion.selected - 1) or 0;
	local wmo = searchWmo and searchWmo.checked or false;

	for _, map in ipairs(allMaps) do
		local visible = name == "" or string.find(string.lower(map.name), name, 1, true) ~= nil;

		if ( visible and typeIndex ~= 0 and map.type ~= typeIndex - 2 ) then
			visible = false;
		end

		if ( visible and expansionIndex ~= 0 and map.expansion ~= expansionIndex - 1 ) then
			visible = false;
		end

		if ( visible and map.wmo ~= wmo ) then
			visible = false;
		end

		if ( visible ) then
			table.insert(filtered, map);
		end
	end

	local selectedId = mapList.items[mapList.selected or 0] and mapList.items[mapList.selected].id;
	NoggitListBox_SetItems(mapList, filtered);

	for index, map in ipairs(filtered) do
		if ( map.id == selectedId ) then
			mapList.selected = index;
		end
	end

	NoggitListBox_Refresh(mapList);
end

function MapEditor_RefreshMapList()
	local pinned = {};
	local rest = {};

	for _, map in ipairs(EditorGetMaps()) do
		if ( map.pinned ) then
			table.insert(pinned, map);
		else
			table.insert(rest, map);
		end
	end

	for _, map in ipairs(rest) do
		table.insert(pinned, map);
	end

	allMaps = pinned;
	FilterMaps();
end

local function RefreshBookmarks()
	NoggitListBox_SetItems(bookmarkList, EditorGetBookmarks());
end

local function MinimapTileSize()
	local holder = minimap.holder;
	local size = min(holder:GetWidth() or 0, holder:GetHeight() or 0);
	return max(2, min(16, floor(size / 64)));
end

local function ShowMinimap(map)
	currentMinimapMap = map;

	if ( not map ) then
		minimap:Hide();
		minimap.placeholder:Show();
		return;
	end

	local tileSize = MinimapTileSize();
	local path, textureSize, tiles, wmo = EditorGetMinimap(map.id, tileSize);

	if ( not path ) then
		minimap:Hide();
		minimap.placeholder:Show();
		return;
	end

	local size = tileSize * 64;
	minimap.tileSize = tileSize;
	minimap.tiles = tiles;
	minimap.wmo = wmo;
	minimap.map = map;
	minimap:SetWidth(size);
	minimap:SetHeight(size);
	minimap.image:SetTexture(path);
	minimap.image:SetTexCoord(0, size / textureSize, 0, size / textureSize);
	minimap.placeholder:Hide();
	minimap:Show();
end

local function MinimapTileAtCursor()
	local x, y = NoggitUI_CursorIn(minimap);
	local tileX = floor((x - minimap:GetLeft()) / minimap.tileSize);
	local tileY = floor((minimap:GetTop() - y) / minimap.tileSize);

	if ( tileX < 0 or tileY < 0 or tileX > 63 or tileY > 63 ) then
		return nil;
	end

	return tileX, tileY;
end

local function EnterMap(entered)
	if ( entered ) then
		startScreen:Hide();
		MapEditor_SetEditorShown(true);
	end
end

local function CreateTabPane(parent, names, disabled)
	local holder = CreateFrame("Frame", NoggitUI_NewName("NoggitTabWidget"), parent);
	local panes = {};

	local tabs = NoggitTabs_Create(holder, names, 1, function(index)
		for i, pane in ipairs(panes) do
			if ( i == index ) then
				pane:Show();
			else
				pane:Hide();
			end
		end
	end, disabled);
	tabs[1]:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, 0);

	for i = 1, #names do
		local pane = CreateFrame("Frame", NoggitUI_NewName("NoggitTabPane"), holder, "NoggitBoxTemplate");
		pane:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, -24);
		pane:SetPoint("BOTTOMRIGHT", holder, "BOTTOMRIGHT", 0, 0);
		NoggitBox_Set(pane, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);

		if ( i ~= 1 ) then
			pane:Hide();
		end

		panes[i] = pane;
	end

	holder.panes = panes;
	holder.tabs = tabs;
	return holder;
end

local function PromptExit(toProjectSelection, onExit)
	local buttons = { { text = "Exit", onClick = onExit } };

	if ( not toProjectSelection ) then
		table.insert(buttons, { text = "Return to menu", onClick = function()
			EditorUnloadMap();
			MapEditor_ShowMapSelection();
		end });
	end

	table.insert(buttons, { text = "Cancel" });
	NoggitMessageBox_Show("Exit?", "Exit?\n\nAny unsaved changes will be lost.", buttons);
end

function MapEditor_ExitToProjectSelection()
	local function exit()
		EditorUnloadMap();
		EditorCloseProject();
		MapEditor_ShowProjectSelection();
	end

	if ( EditorIsMapLoaded() ) then
		PromptExit(true, exit);
	else
		exit();
	end
end

function MapEditor_Exit()
	if ( EditorIsMapLoaded() ) then
		PromptExit(false, EditorQuit);
	else
		EditorQuit();
	end
end

function MapEditor_ShowAbout()
	NoggitMessageBox_Show("About", "Noggit Red\n\nMap editor UI rebuilt on the wowdecomp engine\n(World of Warcraft 3.3.5a, build 12340).", { { text = "OK" } });
end

function MapEditor_NoggitMenu()
	return {
		title = "Noggit",
		{ text = "Settings", disabled = true },
		{ text = "About", onClick = MapEditor_ShowAbout },
		{ text = "Exit to Project Selection", onClick = MapEditor_ExitToProjectSelection },
		{ text = "Exit", onClick = MapEditor_Exit },
	};
end

local function CreateMapScreen()
	mapScreen = CreateFrame("Frame", "EditorMapSelection", startScreen);
	mapScreen:SetAllPoints(startScreen);

	local menuBar = CreateFrame("Frame", "EditorMapSelectionMenuBar", mapScreen, "NoggitBoxTemplate");
	menuBar:SetHeight(26);
	menuBar:SetPoint("TOPLEFT", mapScreen, "TOPLEFT", 0, 0);
	menuBar:SetPoint("TOPRIGHT", mapScreen, "TOPRIGHT", 0, 0);
	NoggitBox_Set(menuBar, NOGGIT_COLOR_BASE, nil);
	NoggitMenuBar_Build(menuBar, { MapEditor_NoggitMenu() });

	local left = CreateTabPane(mapScreen, { "Maps", "Bookmarks" });
	left:SetWidth(310);
	left:SetPoint("TOPLEFT", menuBar, "BOTTOMLEFT", 9, -9);
	left:SetPoint("BOTTOM", mapScreen, "BOTTOM", 0, 9);

	local mapsPane = left.panes[1];
	local group = CreateFrame("Frame", "EditorMapSearchGroup", mapsPane, "NoggitGroupBoxTemplate");
	group:SetPoint("TOPLEFT", mapsPane, "TOPLEFT", 9, -9);
	group:SetPoint("TOPRIGHT", mapsPane, "TOPRIGHT", -9, -9);
	group:SetHeight(150);
	local groupTitle = _G[group:GetName().."Title"];
	groupTitle:SetText("Search");
	_G[group:GetName().."TitleLine"]:SetWidth(groupTitle:GetStringWidth());

	local fieldX = 82;
	local fieldWidth = 310 - 18 - fieldX - 10;
	local y = -28;

	local label = NoggitUI_FontString(group, "Name : ");
	label:SetPoint("TOPLEFT", group, "TOPLEFT", 10, y - 4);
	searchName = NoggitLineEdit_Create(group, fieldWidth);
	searchName:SetPoint("TOPLEFT", group, "TOPLEFT", fieldX, y);
	searchName:SetScript("OnTextChanged", FilterMaps);

	y = y - 28;
	label = NoggitUI_FontString(group, "Type : ");
	label:SetPoint("TOPLEFT", group, "TOPLEFT", 10, y - 4);
	searchType = NoggitComboBox_Create(group, fieldWidth, {
		{ text = "All" }, { text = "Unknown" }, { text = "Continent" }, { text = "Dungeon" }, { text = "Raid" }, { text = "Battleground" }, { text = "Arena" }, { text = "Scenario" },
	}, EditorGetMapComboSearch() + 1, function(_, index)
		EditorSetMapComboSearch(index - 1);
		FilterMaps();
	end);
	searchType:SetPoint("TOPLEFT", group, "TOPLEFT", fieldX, y);

	y = y - 28;
	label = NoggitUI_FontString(group, "Expansion : ");
	label:SetPoint("TOPLEFT", group, "TOPLEFT", 10, y - 4);
	local expansions = { { text = "All" } };
	local expansionNames = { "Classic", "Burning Cursade", "Wrath of the Lich King", "Cataclism", "Mist of Pandaria", "Warlords of Draenor", "Legion", "Battle for Azeroth", "Shadowlands" };

	for index, name in ipairs(expansionNames) do
		table.insert(expansions, { text = name, icon = ExpansionIcon(index - 1) });
	end

	searchExpansion = NoggitComboBox_Create(group, fieldWidth, expansions, 1, FilterMaps);
	searchExpansion:SetPoint("TOPLEFT", group, "TOPLEFT", fieldX, y);

	y = y - 30;
	searchWmo = NoggitCheckBox_Create(group, "Display WMO maps (No terrain)", false, FilterMaps);
	searchWmo:SetPoint("TOPLEFT", group, "TOPLEFT", 10, y);

	local addButton = NoggitPushButton_Create(mapsPane, "    Add New Map", nil);
	addButton:SetPoint("BOTTOMLEFT", mapsPane, "BOTTOMLEFT", 9, 9);
	addButton:SetPoint("BOTTOMRIGHT", mapsPane, "BOTTOMRIGHT", -9, 9);
	local plus = Glyph(addButton, GLYPH_PLUS, "NoggitAwesomeFont", NOGGIT_COLOR_DISABLED);
	plus:SetPoint("RIGHT", addButton:GetFontString(), "LEFT", -2, 0);
	addButton.tooltip = "Map creation wizard (not available yet)";
	addButton:Disable();

	mapList = NoggitListBox_Create(mapsPane, 34, BuildMapRow, RefreshMapRow);
	mapList:SetPoint("TOPLEFT", group, "BOTTOMLEFT", 0, -5);
	mapList:SetPoint("TOPRIGHT", group, "BOTTOMRIGHT", 0, -5);
	mapList:SetPoint("BOTTOM", addButton, "TOP", 0, 6);
	mapList.onSelect = function(map)
		ShowMinimap(map);
	end;
	mapList.onContext = function(map)
		contextTarget = map;

		if ( map.pinned ) then
			NoggitContextMenu_Show("mapPinned", MAP_MENU_PINNED);
		else
			NoggitContextMenu_Show("map", MAP_MENU);
		end
	end;

	bookmarkList = NoggitListBox_Create(left.panes[2], 34, BuildBookmarkRow, RefreshBookmarkRow);
	bookmarkList:SetPoint("TOPLEFT", left.panes[2], "TOPLEFT", 9, -9);
	bookmarkList:SetPoint("BOTTOMRIGHT", left.panes[2], "BOTTOMRIGHT", -9, 9);
	bookmarkList.onDoubleClick = function(_, index)
		EnterMap(EditorEnterBookmark(index));
	end;

	local right = CreateTabPane(mapScreen, { "Enter map", "Edit map" }, { [2] = true });
	right:SetPoint("TOPLEFT", left, "TOPRIGHT", 6, 0);
	right:SetPoint("BOTTOMRIGHT", mapScreen, "BOTTOMRIGHT", -9, 9);
	right.tabs[2].tooltip = "Map creation wizard (not available yet)";

	local holder = CreateFrame("Frame", "EditorMinimapHolder", right.panes[1]);
	holder:SetPoint("TOPLEFT", right.panes[1], "TOPLEFT", 4, -4);
	holder:SetPoint("BOTTOMRIGHT", right.panes[1], "BOTTOMRIGHT", -4, 4);

	minimap = CreateFrame("Button", "EditorMinimap", holder);
	minimap:SetPoint("CENTER", holder, "CENTER", 0, 0);
	minimap:RegisterForClicks("LeftButtonUp");
	minimap.holder = holder;
	minimap.image = minimap:CreateTexture(nil, "ARTWORK");
	minimap.image:SetAllPoints(minimap);
	minimap:Hide();

	minimap.placeholder = NoggitUI_FontString(holder, "Select a map", "NoggitFont30", "CENTER");
	minimap.placeholder:SetPoint("CENTER", holder, "CENTER", 0, 0);

	minimap:SetScript("OnUpdate", function(self)
		if ( not self:IsMouseOver() ) then
			if ( self.tooltipShown ) then
				self.tooltipShown = false;
				NoggitTooltip_Hide();
			end

			return;
		end

		local tileX, tileY = MinimapTileAtCursor();

		if ( tileX ) then
			NoggitTooltip_ShowAtCursor(format("ADT: %d_%d", tileX, tileY));
			self.tooltipShown = true;
		end
	end);

	minimap:SetScript("OnDoubleClick", function(self)
		local tileX, tileY = MinimapTileAtCursor();

		if ( not tileX or not self.map ) then
			return;
		end

		if ( string.sub(self.tiles, tileY * 64 + tileX + 1, tileY * 64 + tileX + 1) ~= "1" and not self.wmo ) then
			return;
		end

		NoggitTooltip_Hide();
		EnterMap(EditorEnterMap(self.map.id, tileX, tileY));
	end);

	holder:SetScript("OnSizeChanged", function()
		if ( currentMinimapMap and minimap.tileSize ~= MinimapTileSize() ) then
			ShowMinimap(currentMinimapMap);
		end
	end);
end

function MapEditor_ShowProjectSelection()
	RefreshProjects();
	ShowStartScreen(projectWindow);
end

function MapEditor_ShowMapSelection()
	if ( not EditorGetProject() ) then
		MapEditor_ShowProjectSelection();
		return;
	end

	ShowStartScreen(mapScreen);
	MapEditor_RefreshMapList();
	RefreshBookmarks();

	if ( currentMinimapMap ) then
		ShowMinimap(currentMinimapMap);
	end
end

function MapEditor_CreateStartScreens()
	startScreen = CreateFrame("Frame", "EditorStartScreen", EditorParent, "NoggitBoxTemplate");
	startScreen:SetAllPoints(EditorParent);
	startScreen:SetFrameStrata("HIGH");
	startScreen:EnableMouse(true);
	startScreen:EnableMouseWheel(true);
	startScreen:Hide();

	CreateProjectWindow();
	CreateCreationDialog();
	CreateMapScreen();
end

function MapEditor_StartFlow()
	local path, favorite = EditorGetStartupProject();

	if ( path ) then
		local ok, message = EditorOpenProject(path);

		if ( ok and message == "relaunch" ) then
			return;
		end

		if ( ok ) then
			MapEditor_ShowMapSelection();
			return;
		end
	end

	MapEditor_ShowProjectSelection();
end
