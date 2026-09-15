#pragma once

// Acrylic (frosted glass) backdrop helper.
// Implemented via user32!SetWindowCompositionAttribute with
// ACCENT_ENABLE_ACRYLICBLURBEHIND (Windows 10 1803+, best on Windows 11).
// Works on borderless custom-drawn windows; only valid for top-level windows.
// Once enabled, the DWM realtime blur behind the window shows through.
namespace Backdrop
{
	// Enable the acrylic blur backdrop for a window.
	// tintColor: tint color (RGB), usually the theme background color;
	//   dark themes produce dark glass, light themes produce light glass.
	// alpha: 0-255, smaller = more transparent / more visible blur. Suggested 120-220.
	// Returns true on success; on failure (unsupported system) caller can fall back.
	bool EnableAcrylic(HWND hWnd, COLORREF tintColor, BYTE alpha);

	// Disable the backdrop effect and restore default opaque rendering.
	void Disable(HWND hWnd);

	// Whether acrylic was successfully enabled in this process
	// (child controls use this to decide whether to show the glass through).
	bool IsEnabled();

	// When acrylic is enabled, fill the window/control background with an
	// alpha-blended theme color so the blur behind shows through.
	// Called from WM_ERASEBKGND; returns false when acrylic is off and the
	// caller should do its normal opaque fill.
	bool FillGlassBackground(HDC hdc, const CRect &rect, COLORREF color);
}
