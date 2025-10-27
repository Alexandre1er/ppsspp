#pragma once

// Copyright (c) 2013- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Common/UI/UIScreen.h"
#include "Common/UI/ViewGroup.h"
#include "UI/MiscScreens.h"

// Upload screen: Shows the user an ip address to go to in a web browser on the same network,
// in order to upload game files to the current directory.

class UploadScreen : public UIDialogScreenWithBackground {
public:
	UploadScreen(const Path &targetFolder);
	~UploadScreen();
	void CreateViews() override;

protected:
	void update() override;

	const char *tag() const override { return "Upload"; }

private:
	bool prevRunning_ = false;
	std::vector<std::string> localIPs_;
	Path targetFolder_;
};
