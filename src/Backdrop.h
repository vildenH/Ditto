#pragma once

// DWM window appearance helpers for the quick paste window.
namespace Backdrop
{
	// Apply Windows 11 rounded window corners (DWMWCP_ROUND).
	// Silently does nothing on older systems.
	void ApplyRoundedCorners(HWND hWnd);
}
