#pragma once

#include <vector>
#include "DPI.h"

// Alfred-style right side preview pane for the Quick Paste window.
// Shows the formats + sizes, a text preview, an image thumbnail
// and a file list for the currently selected clip.
class CPreviewPane : public CWnd
{
public:
	CPreviewPane();
	virtual ~CPreviewPane();

	BOOL Create(CWnd* pParentWnd);

	void SetDpiInfo(CDPI* pDpi) { m_pDpi = pDpi; }

	// Load and display the data for the given clip (Main.lID). Pass -1 to clear.
	void SetClip(int clipId);
	void Clear();

	void SetColors(COLORREF bg, COLORREF text, COLORREF headerText);

protected:
	struct FormatInfo
	{
		CString m_csName;
		__int64 m_nSize;

		FormatInfo() : m_nSize(0) {}
	};

	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	void LoadMetadata(int clipId);
	void LoadTextPreview(int clipId);
	void LoadImagePreview(int clipId, const CString& csFormatName);
	void LoadFileList(int clipId);

	void DrawSectionTitle(CDC& dc, CRect& rc, const CString& csTitle);
	void DrawBodyText(CDC& dc, CRect& rc, const CString& csText);
	CString FormatByteSize(__int64 nSize) const;

	CDPI* m_pDpi;
	int m_clipId;
	bool m_bHasMetadata;

	std::vector<FormatInfo> m_formats;
	__int64 m_nTotalSize;

	CString m_csTextPreview;
	bool m_bTruncatedText;

	Gdiplus::Bitmap* m_pBitmap;
	CString m_csImageFormatName;

	CStringArray m_fileNames;
	bool m_bFileListTruncated;

	COLORREF m_crBg;
	COLORREF m_crText;
	COLORREF m_crHeaderText;

	DECLARE_MESSAGE_MAP()
};
