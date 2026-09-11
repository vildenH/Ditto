#pragma once

#include <vector>
#include "DPI.h"
#include "RichEditCtrlEx.h"

// Alfred-style right side preview pane for the Quick Paste window.
// Built from standard controls (read-only rich edit + static bitmap),
// consistent with how the rest of the Ditto UI is built.
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

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	void LoadMetadata(int clipId);
	void LoadTextPreview(int clipId);
	void LoadImagePreview(int clipId, const CString& csFormatName);
	void LoadFileList(int clipId);

	void UpdateContent();
	void LayoutChildren();
	void ClearImage();
	CString FormatByteSize(__int64 nSize) const;

	CDPI* m_pDpi;
	int m_clipId;
	bool m_bHasMetadata;

	std::vector<FormatInfo> m_formats;
	__int64 m_nTotalSize;

	CString m_csTextPreview;
	bool m_bTruncatedText;

	Gdiplus::Bitmap* m_pBitmap;
	HBITMAP m_hPreviewBmp;
	CString m_csImageFormatName;

	CStringArray m_fileNames;
	bool m_bFileListTruncated;

	COLORREF m_crBg;
	COLORREF m_crText;
	COLORREF m_crHeaderText;

	// standard controls doing all the rendering
	CRichEditCtrlEx m_edit;
	CStatic m_imgStatic;

	DECLARE_MESSAGE_MAP()
};
