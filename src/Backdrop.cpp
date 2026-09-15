#include "StdAfx.h"
#include "Backdrop.h"

namespace
{
	// dwmapi!DwmSetWindowAttribute loaded dynamically, no import lib needed
	typedef HRESULT(WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

	PFN_DwmSetWindowAttribute GetDwmSetWindowAttribute()
	{
		static PFN_DwmSetWindowAttribute pfn = NULL;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			HMODULE hDwm = ::LoadLibrary(_T("dwmapi.dll"));
			if (hDwm != NULL)
			{
				pfn = (PFN_DwmSetWindowAttribute)::GetProcAddress(hDwm, "DwmSetWindowAttribute");
			}
		}
		return pfn;
	}
}

namespace Backdrop
{
	void ApplyRoundedCorners(HWND hWnd)
	{
		PFN_DwmSetWindowAttribute pfn = GetDwmSetWindowAttribute();
		if (pfn == NULL || hWnd == NULL || !::IsWindow(hWnd))
		{
			return;
		}

		// DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2
		int preference = 2;
		pfn(hWnd, 33, &preference, sizeof(preference));
	}
}
