#include "StdAfx.h"
#include "Backdrop.h"

// user32!SetWindowCompositionAttribute is an undocumented API available
// since Windows 10, loaded dynamically.
typedef struct _BACKDROP_ACCENT_POLICY
{
	DWORD AccentState;
	DWORD AccentFlags;
	DWORD GradientColor;	// AABBGGRR
	DWORD AnimationId;
} BACKDROP_ACCENT_POLICY;

typedef struct _BACKDROP_WINDOWCOMPOSITIONATTRIBDATA
{
	DWORD Attrib;
	PVOID pvData;
	SIZE_T cbData;
} BACKDROP_WINDOWCOMPOSITIONATTRIBDATA;

#define BACKDROP_ACCENT_DISABLED					0
#define BACKDROP_ACCENT_ENABLE_ACRYLICBLURBEHIND	4
#define BACKDROP_WCA_ACCENT_POLICY					19

namespace
{
	BOOL SetAccentPolicy(HWND hWnd, const BACKDROP_ACCENT_POLICY &policy)
	{
		static BOOL(WINAPI *pfnSetWindowCompositionAttribute)(HWND, BACKDROP_WINDOWCOMPOSITIONATTRIBDATA *) = NULL;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			HMODULE hUser32 = ::GetModuleHandle(_T("user32.dll"));
			if (hUser32 != NULL)
			{
				*(FARPROC *)&pfnSetWindowCompositionAttribute = ::GetProcAddress(hUser32, "SetWindowCompositionAttribute");
			}
		}

		if (pfnSetWindowCompositionAttribute == NULL)
		{
			return FALSE;
		}

		BACKDROP_WINDOWCOMPOSITIONATTRIBDATA data;
		data.Attrib = BACKDROP_WCA_ACCENT_POLICY;
		data.pvData = (PVOID)&policy;
		data.cbData = sizeof(policy);
		return pfnSetWindowCompositionAttribute(hWnd, &data);
	}

	// Whether acrylic was successfully enabled (only the main quick paste
	// window uses it in this app).
	bool g_bAcrylicEnabled = false;
}

namespace Backdrop
{
	bool EnableAcrylic(HWND hWnd, COLORREF tintColor, BYTE alpha)
	{
		if (hWnd == NULL || !::IsWindow(hWnd))
		{
			return false;
		}

		BACKDROP_ACCENT_POLICY policy = {};
		policy.AccentState = BACKDROP_ACCENT_ENABLE_ACRYLICBLURBEHIND;
		policy.AccentFlags = 0;
		policy.GradientColor = ((DWORD)alpha << 24) | ((DWORD)tintColor & 0x00FFFFFF);
		policy.AnimationId = 0;

		bool bOk = SetAccentPolicy(hWnd, policy) != FALSE;
		if (bOk)
		{
			g_bAcrylicEnabled = true;
		}
		return bOk;
	}

	void Disable(HWND hWnd)
	{
		BACKDROP_ACCENT_POLICY policy = {};
		policy.AccentState = BACKDROP_ACCENT_DISABLED;
		SetAccentPolicy(hWnd, policy);
		g_bAcrylicEnabled = false;
	}

	bool IsEnabled()
	{
		return g_bAcrylicEnabled;
	}

	bool FillGlassBackground(HDC hdc, const CRect &rect, COLORREF color)
	{
		if (!g_bAcrylicEnabled || hdc == NULL)
		{
			return false;
		}

		// Use GDI+ to write an alpha-blended solid fill. Plain GDI writes
		// opaque pixels into the DWM redirection surface, only GDI+ can
		// write semi-transparency so the acrylic blur shows through.
		Gdiplus::Graphics graphics(hdc);
		BYTE alpha = 170;
		Gdiplus::SolidBrush brush(Gdiplus::Color(alpha,
			GetRValue(color), GetGValue(color), GetBValue(color)));
		graphics.FillRectangle(&brush, Gdiplus::Rect(rect.left, rect.top, rect.Width(), rect.Height()));
		return true;
	}
}
