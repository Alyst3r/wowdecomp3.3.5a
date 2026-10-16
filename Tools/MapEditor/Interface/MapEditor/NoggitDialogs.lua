local TEXTURE_PATH = "Interface\\MapEditor\\Textures\\";
local DOUBLE_CLICK_TIME = 0.4;

NOGGIT_COLOR_GOLD = { 1, 0.8, 0, 1 };
NOGGIT_COLOR_INFO = { 0.843, 0.843, 0.843, 0.5 };

function NoggitUI_CursorIn(frame)
	local x, y = EditorGetCursorPosition();
	local scale = frame:GetEffectiveScale();
	return x / scale, y / scale;
end

function NoggitUI_CamelCase(text)
	return (string.gsub(text or "", "(%S)(%S*)", function(first, rest)
		return string.upper(first)..rest;
	end));
end

function NoggitTooltip_ShowAtCursor(text)
	NoggitTooltip_Show(EditorParent, text);

	if ( NoggitTooltip ) then
		local x, y = NoggitUI_CursorIn(EditorParent);
		NoggitTooltip:ClearAllPoints();
		NoggitTooltip:SetPoint("TOPLEFT", EditorParent, "BOTTOMLEFT", x, y - 5);
	end
end

local function RowRefresh(list, row)
	local item = list.items[row.itemIndex or 0];

	if ( not item ) then
		row:Hide();
		return;
	end

	row:Show();

	if ( row.itemIndex == list.selected ) then
		row.bg:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 0.55);
	elseif ( row.hover ) then
		row.bg:SetTexture(NOGGIT_COLOR_HOVER[1], NOGGIT_COLOR_HOVER[2], NOGGIT_COLOR_HOVER[3], 1);
	else
		row.bg:SetTexture(0, 0, 0, 0);
	end

	list.refreshRow(row, item, row.itemIndex == list.selected);
end

function NoggitListBox_Refresh(list)
	local visible = #list.rows;
	local maxOffset = max(0, #list.items - list.visibleCount);
	list.offset = max(0, min(list.offset, maxOffset));

	for i, row in ipairs(list.rows) do
		row.itemIndex = list.offset + i;

		if ( i > list.visibleCount ) then
			row.itemIndex = nil;
		end

		RowRefresh(list, row);
	end

	if ( maxOffset > 0 ) then
		local trackHeight = list.track:GetHeight();
		local thumbHeight = max(20, trackHeight * list.visibleCount / #list.items);
		local top = (trackHeight - thumbHeight) * list.offset / maxOffset;
		list.thumb:ClearAllPoints();
		list.thumb:SetPoint("TOPLEFT", list.track, "TOPLEFT", 0, -top);
		list.thumb:SetPoint("TOPRIGHT", list.track, "TOPRIGHT", 0, -top);
		list.thumb:SetHeight(thumbHeight);
		list.track:Show();
	else
		list.track:Hide();
	end
end

local function RowClicked(row, button)
	local list = row.list;
	local index = row.itemIndex;
	local item = index and list.items[index];

	if ( not item ) then
		return;
	end

	local now = EditorGetTime();
	local changed = list.selected ~= index;
	list.selected = index;
	NoggitListBox_Refresh(list);

	if ( changed and list.onSelect ) then
		list.onSelect(item, index);
	end

	if ( button == "RightButton" ) then
		if ( list.onContext ) then
			list.onContext(item, index);
		end

		return;
	end

	if ( list.lastClickIndex == index and now - (list.lastClickTime or 0) < DOUBLE_CLICK_TIME ) then
		list.lastClickTime = 0;

		if ( list.onDoubleClick ) then
			list.onDoubleClick(item, index);
		end

		return;
	end

	list.lastClickIndex = index;
	list.lastClickTime = now;
end

local function CreateRow(list)
	local row = CreateFrame("Button", NoggitUI_NewName("NoggitListRow"), list);
	row:SetHeight(list.rowHeight);
	row:RegisterForClicks("LeftButtonUp", "RightButtonUp");
	row.list = list;
	row.bg = row:CreateTexture(nil, "BACKGROUND");
	row.bg:SetAllPoints(row);
	row.bg:SetTexture(0, 0, 0, 0);
	row:SetScript("OnClick", RowClicked);
	row:SetScript("OnDoubleClick", RowClicked);
	row:SetScript("OnEnter", function(self)
		self.hover = true;
		RowRefresh(list, self);
	end);
	row:SetScript("OnLeave", function(self)
		self.hover = false;
		RowRefresh(list, self);
	end);
	row:EnableMouseWheel(true);
	row:SetScript("OnMouseWheel", function(_, delta)
		list.offset = list.offset - delta;
		NoggitListBox_Refresh(list);
	end);
	list.buildRow(row);
	return row;
end

function NoggitListBox_Layout(list)
	local height = list:GetHeight() or 0;
	list.visibleCount = max(0, floor((height - 4) / list.rowHeight));

	while ( #list.rows < list.visibleCount ) do
		local row = CreateRow(list);
		local index = #list.rows;
		row:SetPoint("TOPLEFT", list, "TOPLEFT", 2, -2 - index * list.rowHeight);
		row:SetPoint("TOPRIGHT", list, "TOPRIGHT", -12, -2 - index * list.rowHeight);
		table.insert(list.rows, row);
	end

	NoggitListBox_Refresh(list);
end

function NoggitListBox_SetItems(list, items, keepSelection)
	list.items = items or {};

	if ( not keepSelection ) then
		list.selected = nil;
		list.offset = 0;
	end

	NoggitListBox_Layout(list);
end

function NoggitListBox_Create(parent, rowHeight, buildRow, refreshRow)
	local list = CreateFrame("Frame", NoggitUI_NewName("NoggitListBox"), parent, "NoggitBoxTemplate");
	NoggitBox_Set(list, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	list:EnableMouse(true);
	list:EnableMouseWheel(true);
	list.rowHeight = rowHeight;
	list.buildRow = buildRow;
	list.refreshRow = refreshRow;
	list.rows = {};
	list.items = {};
	list.offset = 0;
	list.visibleCount = 0;

	local track = CreateFrame("Frame", NoggitUI_NewName("NoggitListTrack"), list);
	track:SetWidth(8);
	track:SetPoint("TOPRIGHT", list, "TOPRIGHT", -2, -2);
	track:SetPoint("BOTTOMRIGHT", list, "BOTTOMRIGHT", -2, 2);
	track:EnableMouse(true);
	NoggitUI_SolidTexture(track, "BACKGROUND", NOGGIT_COLOR_BASE):SetAllPoints(track);
	list.thumb = NoggitUI_SolidTexture(track, "ARTWORK", NOGGIT_COLOR_HOVER);
	list.track = track;

	track:SetScript("OnMouseDown", function()
		list.dragging = true;
	end);
	track:SetScript("OnMouseUp", function()
		list.dragging = false;
	end);

	list:SetScript("OnUpdate", function(self)
		if ( not self.dragging ) then
			return;
		end

		if ( not EditorIsMouseButtonDown() ) then
			self.dragging = false;
			return;
		end

		local _, y = NoggitUI_CursorIn(track);
		local fraction = (track:GetTop() - y) / max(1, track:GetHeight());
		self.offset = floor(fraction * #self.items - self.visibleCount / 2 + 0.5);
		NoggitListBox_Refresh(self);
	end);

	list:SetScript("OnMouseWheel", function(self, delta)
		self.offset = self.offset - delta;
		NoggitListBox_Refresh(self);
	end);

	list:SetScript("OnSizeChanged", NoggitListBox_Layout);
	return list;
end

local comboPopup;
local comboRows = {};

local function CloseComboPopup()
	if ( comboPopup ) then
		comboPopup:Hide();
		comboPopup.owner = nil;
	end
end

function NoggitComboBox_SetSelected(combo, index, quiet)
	local item = combo.items[index];

	if ( not item ) then
		return;
	end

	combo.selected = index;
	combo.text:SetText(item.text);

	if ( item.icon ) then
		combo.icon:SetTexture(item.icon);
		combo.icon:Show();
		combo.text:SetPoint("LEFT", combo, "LEFT", 28, 0);
	else
		combo.icon:Hide();
		combo.text:SetPoint("LEFT", combo, "LEFT", 8, 0);
	end

	if ( not quiet and combo.onChange ) then
		combo.onChange(combo, index, item);
	end
end

local function OpenComboPopup(combo)
	if ( not comboPopup ) then
		comboPopup = CreateFrame("Frame", "NoggitComboPopup", EditorParent, "NoggitBoxTemplate");
		comboPopup:SetFrameStrata("TOOLTIP");
		comboPopup:EnableMouse(true);
		NoggitBox_Set(comboPopup, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);
		comboPopup:SetScript("OnUpdate", function(self)
			local down = EditorIsMouseButtonDown();

			if ( down and not self.wasDown and not self:IsMouseOver() and not (self.owner and self.owner:IsMouseOver()) ) then
				CloseComboPopup();
			end

			self.wasDown = down;
		end);
	end

	if ( comboPopup:IsShown() and comboPopup.owner == combo ) then
		CloseComboPopup();
		return;
	end

	comboPopup.owner = combo;
	comboPopup.wasDown = true;

	for i, item in ipairs(combo.items) do
		local row = comboRows[i];

		if ( not row ) then
			row = CreateFrame("Button", NoggitUI_NewName("NoggitComboRow"), comboPopup);
			row:SetHeight(22);
			row.bg = row:CreateTexture(nil, "BACKGROUND");
			row.bg:SetAllPoints(row);
			row.icon = row:CreateTexture(nil, "ARTWORK");
			row.icon:SetWidth(16);
			row.icon:SetHeight(16);
			row.icon:SetPoint("LEFT", row, "LEFT", 6, 0);
			row.text = NoggitUI_FontString(row, "");
			row:SetScript("OnEnter", function(self)
				self.bg:SetTexture(NOGGIT_COLOR_HOVER[1], NOGGIT_COLOR_HOVER[2], NOGGIT_COLOR_HOVER[3], 1);
			end);
			row:SetScript("OnLeave", function(self)
				self.bg:SetTexture(0, 0, 0, 0);
			end);
			row:SetScript("OnClick", function(self)
				local owner = comboPopup.owner;
				CloseComboPopup();

				if ( owner ) then
					NoggitComboBox_SetSelected(owner, self.index);
				end
			end);
			comboRows[i] = row;
		end

		row.index = i;
		row:ClearAllPoints();
		row:SetPoint("TOPLEFT", comboPopup, "TOPLEFT", 1, -1 - (i - 1) * 22);
		row:SetPoint("TOPRIGHT", comboPopup, "TOPRIGHT", -1, -1 - (i - 1) * 22);
		row.bg:SetTexture(0, 0, 0, 0);
		row.text:SetText(item.text);
		row.text:ClearAllPoints();

		if ( item.icon ) then
			row.icon:SetTexture(item.icon);
			row.icon:Show();
			row.text:SetPoint("LEFT", row, "LEFT", 28, 0);
		else
			row.icon:Hide();
			row.text:SetPoint("LEFT", row, "LEFT", 8, 0);
		end

		row:Show();
	end

	for i = #combo.items + 1, #comboRows do
		comboRows[i]:Hide();
	end

	comboPopup:ClearAllPoints();
	comboPopup:SetPoint("TOPLEFT", combo, "BOTTOMLEFT", 0, 0);
	comboPopup:SetWidth(combo:GetWidth());
	comboPopup:SetHeight(#combo.items * 22 + 2);
	comboPopup:Show();
end

function NoggitComboBox_Create(parent, width, items, selected, onChange)
	local combo = CreateFrame("Button", NoggitUI_NewName("NoggitComboBox"), parent, "NoggitBoxTemplate");
	combo:SetWidth(width);
	combo:SetHeight(22);
	combo.items = items;
	combo.onChange = onChange;
	NoggitBox_Set(combo, NOGGIT_COLOR_MID, NOGGIT_COLOR_HOVER);

	combo.icon = combo:CreateTexture(nil, "ARTWORK");
	combo.icon:SetWidth(16);
	combo.icon:SetHeight(16);
	combo.icon:SetPoint("LEFT", combo, "LEFT", 6, 0);

	combo.text = NoggitUI_FontString(combo, "");
	combo.text:SetPoint("LEFT", combo, "LEFT", 8, 0);

	local arrow = combo:CreateTexture(nil, "ARTWORK");
	arrow:SetTexture(TEXTURE_PATH.."down_arrow.tga");
	arrow:SetWidth(8);
	arrow:SetHeight(8);
	arrow:SetPoint("RIGHT", combo, "RIGHT", -8, 0);

	combo:SetScript("OnEnter", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_HOVER, NOGGIT_COLOR_HOVER);
	end);
	combo:SetScript("OnLeave", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_MID, NOGGIT_COLOR_HOVER);
	end);
	combo:SetScript("OnClick", OpenComboPopup);

	NoggitComboBox_SetSelected(combo, selected or 1, true);
	return combo;
end

local messageBox;
local messageButtons = {};

function NoggitMessageBox_Hide()
	if ( messageBox ) then
		messageBox:Hide();
	end
end

function NoggitMessageBox_Show(title, text, buttons)
	if ( not messageBox ) then
		messageBox = CreateFrame("Frame", "NoggitMessageBox", EditorParent);
		messageBox:SetAllPoints(EditorParent);
		messageBox:SetFrameStrata("FULLSCREEN");
		messageBox:EnableMouse(true);
		messageBox:EnableMouseWheel(true);
		NoggitUI_SolidTexture(messageBox, "BACKGROUND", { 0, 0, 0, 0.35 }):SetAllPoints(messageBox);

		local box = CreateFrame("Frame", "NoggitMessageBoxWindow", messageBox, "NoggitBoxTemplate");
		box:SetPoint("CENTER", messageBox, "CENTER", 0, 40);
		box:EnableMouse(true);
		NoggitBox_Set(box, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);
		messageBox.box = box;

		local titleBar = NoggitUI_SolidTexture(box, "BORDER", NOGGIT_COLOR_DARK);
		titleBar:SetPoint("TOPLEFT", box, "TOPLEFT", 1, -1);
		titleBar:SetPoint("TOPRIGHT", box, "TOPRIGHT", -1, -1);
		titleBar:SetHeight(28);

		messageBox.title = NoggitUI_FontString(box, "");
		messageBox.title:SetPoint("TOPLEFT", box, "TOPLEFT", 12, -8);

		messageBox.icon = NoggitUI_FontString(box, "!", "NoggitFont30", "CENTER");
		messageBox.icon:SetPoint("TOPLEFT", box, "TOPLEFT", 18, -44);
		messageBox.icon:SetWidth(30);

		messageBox.text = NoggitUI_FontString(box, "");
		messageBox.text:SetPoint("TOPLEFT", box, "TOPLEFT", 62, -46);
		messageBox.text:SetWidth(360);
	end

	local box = messageBox.box;
	messageBox.text:SetText(text);
	messageBox.title:SetText(title or "");
	local textHeight = max(30, messageBox.text:GetHeight());
	local width = 440;

	for _, button in ipairs(messageButtons) do
		button:Hide();
	end

	local previous;
	buttons = buttons or { { text = "OK" } };

	for i = #buttons, 1, -1 do
		local definition = buttons[i];
		local button = messageButtons[i];

		if ( not button ) then
			button = NoggitPushButton_Create(box, "", 80);
			messageButtons[i] = button;
		end

		button:SetText(definition.text);
		button:SetWidth(max(80, button:GetFontString():GetStringWidth() + 24));
		button:ClearAllPoints();

		if ( previous ) then
			button:SetPoint("RIGHT", previous, "LEFT", -6, 0);
		else
			button:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -12, 12);
		end

		button:SetScript("OnClick", function()
			NoggitMessageBox_Hide();

			if ( definition.onClick ) then
				definition.onClick();
			end
		end);
		button:Show();
		previous = button;
	end

	box:SetWidth(width);
	box:SetHeight(46 + textHeight + 20 + 23 + 12);
	messageBox:Show();
end

function NoggitMessageBox_Critical(text)
	NoggitMessageBox_Show("Error", text, { { text = "OK" } });
end

function NoggitSeparator_Create(parent, vertical)
	local line = NoggitUI_SolidTexture(parent, "ARTWORK", NOGGIT_COLOR_HOVER);

	if ( vertical ) then
		line:SetWidth(1);
	else
		line:SetHeight(1);
	end

	return line;
end
