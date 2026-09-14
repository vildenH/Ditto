#pragma once

// Borderless Quick-Look-style viewer that shows the original image of a
// clip centered on a dimmed full-monitor overlay. Closed by Esc, Space,
// Enter, a click anywhere, or when it loses activation.
class CImageViewerWnd : public CWnd
{
public:
	// Loads the original image for the clip (Main.lID) and shows the viewer
	// on the monitor containing pRefWnd. Returns false when the clip has no
	// loadable image.
	static bool ShowForClip(int clipId, CWnd* pRefWnd);

protected:
	CImageViewerWnd();
	virtual ~CImageViewerWnd();

	bool LoadFromClip(int clipId);
	void Close();

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnDestroy();
	virtual void PostNcDestroy();

	Gdiplus::Bitmap* m_pImage;
	CFont m_hintFont;

	// only one viewer at a time
	static CImageViewerWnd* s_pInstance;

	DECLARE_MESSAGE_MAP()
};
