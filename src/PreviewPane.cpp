#include "stdafx.h"
#include "PreviewPane.h"
#include "CP_Main.h"
#include "Misc.h"
#include "ImageHelper.h"
#include "Sqlite\CppSQLite3.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define PREVIEW_TEXT_MAX_BYTES	4000
#define PREVIEW_MAX_FILES		12

#define ID_PREVIEW_EDIT			100
#define ID_PREVIEW_IMAGE		101

BEGIN_MESSAGE_MAP(CPreviewPane, CWnd)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CPreviewPane::CPreviewPane() :
	m_pDpi(NULL),
	m_clipId(-1),
	m_bHasMetadata(false),
	m_nTotalSize(0),
	m_bTruncatedText(false),
	m_pBitmap(NULL),
	m_hPreviewBmp(NULL),
	m_bFileListTruncated(false),
	m_crBg(RGB(255, 255, 255)),
	m_crText(RGB(0, 0, 0)),
	m_crHeaderText(RGB(64, 64, 64))
{
}

CPreviewPane::~CPreviewPane()
{
	ClearImage();
	if (m_pBitmap)
	{
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
}

BOOL CPreviewPane::Create(CWnd* pParentWnd)
{
	DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
	CRect rc(0, 0, 0, 0);
	if (CreateEx(0, NULL, _T(""), dwStyle, rc, pParentWnd, 0) == FALSE)
	{
		return FALSE;
	}

	// read-only rich edit displaying header / formats / files / content
	DWORD dwEditStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL;
	if (m_edit.Create(NULL, NULL, dwEditStyle, CRect(0, 0, 0, 0), this, ID_PREVIEW_EDIT) == FALSE)
	{
		return FALSE;
	}
	UpdateFont();

	// standard static showing the image thumbnail when the clip is an image
	if (m_imgStatic.Create(_T(""), WS_CHILD | SS_BITMAP, CRect(0, 0, 0, 0), this, ID_PREVIEW_IMAGE) == FALSE)
	{
		return FALSE;
	}

	m_edit.SetBackgroundColor(FALSE, m_crBg);

	CHARFORMAT cf;
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;
	cf.crTextColor = m_crText;
	m_edit.SetDefaultCharFormat(cf);

	return TRUE;
}

void CPreviewPane::SetColors(COLORREF bg, COLORREF text, COLORREF headerText)
{
	m_crBg = bg;
	m_crText = text;
	m_crHeaderText = headerText;

	if (::IsWindow(m_edit.GetSafeHwnd()))
	{
		m_edit.SetBackgroundColor(FALSE, m_crBg);

		CHARFORMAT cf;
		memset(&cf, 0, sizeof(cf));
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_COLOR;
		cf.dwEffects = 0;
		cf.crTextColor = m_crText;
		m_edit.SetDefaultCharFormat(cf);
	}
}

void CPreviewPane::ClearImage()
{
	if (m_hPreviewBmp)
	{
		::DeleteObject(m_hPreviewBmp);
		m_hPreviewBmp = NULL;
	}
}

void CPreviewPane::SetClip(int clipId)
{
	if (clipId == m_clipId)
	{
		return;
	}

	m_clipId = clipId;

	// reset state
	m_bHasMetadata = false;
	m_formats.clear();
	m_nTotalSize = 0;
	m_csTextPreview.Empty();
	m_bTruncatedText = false;
	m_fileNames.RemoveAll();
	m_bFileListTruncated = false;
	if (m_pBitmap)
	{
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
	ClearImage();
	m_csImageFormatName.Empty();

	if (clipId > 0)
	{
		LoadMetadata(clipId);
		LoadTextPreview(clipId);
		LoadFileList(clipId);
		// image loaded in LoadMetadata after we know which image format exists
	}

	UpdateContent();
	LayoutChildren();
}

void CPreviewPane::Clear()
{
	SetClip(-1);
}

void CPreviewPane::LoadMetadata(int clipId)
{
	try
	{
		CString csSQL;
		csSQL.Format(_T("SELECT strClipBoardFormat, length(ooData) AS nDataSize ")
			_T("FROM Data WHERE lParentID = %d ORDER BY Data.lID desc"), clipId);

		CppSQLite3Query q = theApp.m_db.execQuery(csSQL);

		CString csImageFormat;
		while (q.eof() == false)
		{
			FormatInfo fi;
			fi.m_csName = q.getStringField(_T("strClipBoardFormat"));
			fi.m_nSize = q.getIntField(_T("nDataSize"));
			m_nTotalSize += fi.m_nSize;
			m_formats.push_back(fi);

			if (csImageFormat.IsEmpty() &&
				(fi.m_csName.CompareNoCase(_T("CF_DIB")) == 0 ||
				 fi.m_csName.CompareNoCase(_T("CF_DIBV5")) == 0 ||
				 fi.m_csName.CompareNoCase(_T("PNG")) == 0 ||
				 fi.m_csName.CompareNoCase(_T("CF_BITMAP")) == 0))
			{
				csImageFormat = fi.m_csName;
			}

			q.nextRow();
		}

		m_bHasMetadata = true;

		if (csImageFormat.IsEmpty() == FALSE)
		{
			LoadImagePreview(clipId, csImageFormat);
		}
	}
	CATCH_SQLITE_EXCEPTION
}

void CPreviewPane::LoadTextPreview(int clipId)
{
	try
	{
		CString csSQL;
		csSQL.Format(_T("SELECT strClipBoardFormat, ooData FROM Data ")
			_T("WHERE lParentID = %d AND (strClipBoardFormat = 'CF_UNICODETEXT' OR strClipBoardFormat = 'CF_TEXT') ")
			_T("ORDER BY Data.lID desc LIMIT 1"), clipId);

		CppSQLite3Query q = theApp.m_db.execQuery(csSQL);
		if (q.eof() == false)
		{
			CString csFormat = q.getStringField(_T("strClipBoardFormat"));
			int nLen = 0;
			const unsigned char* pData = q.getBlobField(_T("ooData"), nLen);
			if (pData != NULL && nLen > 0)
			{
				int nMax = nLen < PREVIEW_TEXT_MAX_BYTES ? nLen : PREVIEW_TEXT_MAX_BYTES;
				m_bTruncatedText = nLen > nMax;

				if (csFormat.CompareNoCase(_T("CF_UNICODETEXT")) == 0)
				{
					int nChars = nMax / sizeof(wchar_t);
					wchar_t* pText = new wchar_t[nChars + 1];
					memcpy(pText, pData, nChars * sizeof(wchar_t));
					pText[nChars] = 0;
					m_csTextPreview = pText;
					delete[] pText;
				}
				else
				{
					int nChars = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pData, nMax, NULL, 0);
					if (nChars > 0)
					{
						wchar_t* pText = new wchar_t[nChars + 1];
						MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pData, nMax, pText, nChars);
						pText[nChars] = 0;
						m_csTextPreview = pText;
						delete[] pText;
					}
				}
			}
		}
	}
	CATCH_SQLITE_EXCEPTION
}

void CPreviewPane::LoadImagePreview(int clipId, const CString& csFormatName)
{
	try
	{
		CString csSQL;
		csSQL.Format(_T("SELECT ooData FROM Data ")
			_T("WHERE lParentID = %d AND strClipBoardFormat = '%s' ")
			_T("ORDER BY Data.lID desc LIMIT 1"), clipId, csFormatName);

		CppSQLite3Query q = theApp.m_db.execQuery(csSQL);
		if (q.eof() == false)
		{
			int nLen = 0;
			const unsigned char* pData = q.getBlobField(_T("ooData"), nLen);
			if (pData != NULL && nLen > 0)
			{
				HGLOBAL hGlobal = NewGlobalP((LPVOID)pData, nLen);
				if (hGlobal)
				{
					if (csFormatName.CompareNoCase(_T("PNG")) == 0)
					{
						m_pBitmap = PNGImageHelper::GdipImageFromHGLOBAL(hGlobal);
					}
					else
					{
						m_pBitmap = DIBImageHelper::GdipImageFromHGLOBAL(hGlobal);
					}
					GlobalFree(hGlobal);

					// convert to a standard HBITMAP for the SS_BITMAP static control
					if (m_pBitmap &&
						m_pBitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &m_hPreviewBmp) != Gdiplus::Ok)
					{
						m_hPreviewBmp = NULL;
					}
				}

				m_csImageFormatName = csFormatName;
			}
		}
	}
	CATCH_SQLITE_EXCEPTION
}

void CPreviewPane::LoadFileList(int clipId)
{
	try
	{
		CString csSQL;
		csSQL.Format(_T("SELECT ooData FROM Data ")
			_T("WHERE lParentID = %d AND strClipBoardFormat = 'CF_HDROP' ")
			_T("ORDER BY Data.lID desc LIMIT 1"), clipId);

		CppSQLite3Query q = theApp.m_db.execQuery(csSQL);
		if (q.eof() == false)
		{
			int nLen = 0;
			const unsigned char* pData = q.getBlobField(_T("ooData"), nLen);
			if (pData != NULL && nLen > (int)sizeof(DROPFILES))
			{
				DROPFILES* pDrop = (DROPFILES*)pData;
				if (pDrop->pFiles < (UINT)nLen)
				{
					LPBYTE pFiles = (LPBYTE)pData + pDrop->pFiles;
					if (pDrop->fWide)
					{
						wchar_t* pStr = (wchar_t*)pFiles;
						while (*pStr && m_fileNames.GetCount() < PREVIEW_MAX_FILES)
						{
							m_fileNames.Add(CString(pStr));
							pStr += wcslen(pStr) + 1;
						}
						if (*pStr)
						{
							m_bFileListTruncated = true;
						}
					}
					else
					{
						char* pStr = (char*)pFiles;
						while (*pStr && m_fileNames.GetCount() < PREVIEW_MAX_FILES)
						{
							m_fileNames.Add(CString(pStr));
							pStr += strlen(pStr) + 1;
						}
						if (*pStr)
						{
							m_bFileListTruncated = true;
						}
					}
				}
			}
		}
	}
	CATCH_SQLITE_EXCEPTION
}

CString CPreviewPane::FormatByteSize(__int64 nSize) const
{
	TCHAR szSize[128];
	StrFormatByteSize(nSize, szSize, 128);
	return CString(szSize);
}

CString CPreviewPane::GetFriendlyTypeName() const
{
	bool bText = false, bImage = false, bFiles = false, bRtf = false, bHtml = false;
	for (size_t i = 0; i < m_formats.size(); i++)
	{
		const CString& name = m_formats[i].m_csName;
		if (name.CompareNoCase(_T("CF_UNICODETEXT")) == 0 ||
			name.CompareNoCase(_T("CF_TEXT")) == 0)
		{
			bText = true;
		}
		else if (name.CompareNoCase(_T("CF_DIB")) == 0 ||
			name.CompareNoCase(_T("CF_DIBV5")) == 0 ||
			name.CompareNoCase(_T("PNG")) == 0 ||
			name.CompareNoCase(_T("CF_BITMAP")) == 0)
		{
			bImage = true;
		}
		else if (name.CompareNoCase(_T("CF_HDROP")) == 0)
		{
			bFiles = true;
		}
		else if (name.CompareNoCase(_T("RTF")) == 0)
		{
			bRtf = true;
		}
		else if (name.CompareNoCase(_T("HTML Format")) == 0)
		{
			bHtml = true;
		}
	}

	if (bFiles)
	{
		return theApp.m_Language.GetString("PreviewTypeFiles", "Files");
	}
	if (bImage)
	{
		return theApp.m_Language.GetString("PreviewTypeImage", "Image");
	}
	if (bRtf && !bText)
	{
		return theApp.m_Language.GetString("PreviewTypeRichText", "Rich text");
	}
	if (bHtml && !bText)
	{
		return theApp.m_Language.GetString("PreviewTypeWebPage", "Web page");
	}
	return theApp.m_Language.GetString("PreviewTypeText", "Text");
}

void CPreviewPane::UpdateFont()
{
	if (::IsWindow(m_edit.GetSafeHwnd()) == FALSE)
	{
		return;
	}

	// use the same user-configurable font as the main list, DPI scaled
	LOGFONT lf;
	CGetSetOptions::GetFont(lf);
	lf.lfHeight = m_pDpi ? m_pDpi->Scale(lf.lfHeight) : lf.lfHeight;

	m_Font.DeleteObject();
	m_Font.CreateFontIndirect(&lf);
	m_edit.SetFont(&m_Font);
}

void CPreviewPane::UpdateContent()
{
	if (::IsWindow(m_edit.GetSafeHwnd()) == FALSE)
	{
		return;
	}

	if (m_clipId <= 0)
	{
		m_edit.SetWindowText(theApp.m_Language.GetString("PreviewNoItemSelected", "No item selected"));
		return;
	}

	// Alfred-style: one small gray summary line, then the actual content
	CString csType = GetFriendlyTypeName();
	CString csMeta;
	if (csType == theApp.m_Language.GetString("PreviewTypeFiles", "Files"))
	{
		CString csFmt = theApp.m_Language.GetString("PreviewFilesSummary", "%d files · %s");
		csMeta.Format(csFmt, m_fileNames.GetCount(), FormatByteSize(m_nTotalSize));
	}
	else
	{
		csMeta.Format(_T("%s · %s"), csType, FormatByteSize(m_nTotalSize));
	}

	CString cs = csMeta;
	cs += _T("\r\n");

	if (m_fileNames.GetCount() > 0)
	{
		cs += _T("\r\n");
		for (int i = 0; i < m_fileNames.GetCount(); i++)
		{
			cs += m_fileNames[i];
			cs += _T("\r\n");
		}
		if (m_bFileListTruncated)
		{
			cs += _T("...\r\n");
		}
	}

	if (m_csTextPreview.IsEmpty() == FALSE)
	{
		cs += _T("\r\n");
		cs += m_csTextPreview;
		if (m_bTruncatedText)
		{
			cs += _T("\r\n...");
		}
	}

	m_edit.SetWindowText(cs);

	// render the summary line small and gray, the content normal
	CHARRANGE cr;
	cr.cpMin = 0;
	cr.cpMax = csMeta.GetLength();
	m_edit.SetSel(cr);

	CHARFORMAT cf;
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;
	cf.crTextColor = m_crHeaderText;
	m_edit.SetSelectionCharFormat(cf);

	m_edit.SetSel(-1, -1);
}

void CPreviewPane::LayoutChildren()
{
	if (::IsWindow(m_edit.GetSafeHwnd()) == FALSE)
	{
		return;
	}

	CRect rcClient;
	GetClientRect(rcClient);

	int margin = m_pDpi ? m_pDpi->Scale(4) : 4;
	rcClient.DeflateRect(margin, margin, margin, margin);

	// show the image thumbnail on top when available
	CRect rcImage(0, 0, 0, 0);
	CRect rcEdit = rcClient;

	if (m_hPreviewBmp != NULL && m_pBitmap != NULL)
	{
		BITMAP bm;
		if (::GetObject(m_hPreviewBmp, sizeof(bm), &bm) > 0 &&
			bm.bmWidth > 0 && bm.bmHeight > 0)
		{
			double dScale = min((double)rcClient.Width() / bm.bmWidth, 1.0);
			int nDrawWidth = max(1, (int)(bm.bmWidth * dScale));
			int nDrawHeight = max(1, (int)(bm.bmHeight * dScale));

			// keep the thumbnail from eating the whole pane
			int nMaxHeight = rcClient.Height() / 2;
			if (nDrawHeight > nMaxHeight)
			{
				dScale = (double)nMaxHeight / nDrawHeight;
				nDrawWidth = max(1, (int)(nDrawWidth * dScale));
				nDrawHeight = nMaxHeight;
			}

			rcImage = CRect(rcClient.left, rcClient.top, rcClient.left + nDrawWidth, rcClient.top + nDrawHeight);
			rcEdit.top = rcImage.bottom + (m_pDpi ? m_pDpi->Scale(4) : 4);
		}
	}

	if (m_hPreviewBmp != NULL && rcImage.Height() > 0)
	{
		m_imgStatic.SetBitmap(m_hPreviewBmp);
		m_imgStatic.MoveWindow(rcImage);
		m_imgStatic.ShowWindow(SW_SHOW);
	}
	else
	{
		m_imgStatic.ShowWindow(SW_HIDE);
		m_imgStatic.SetBitmap(NULL);
	}

	m_edit.MoveWindow(rcEdit);
}

BOOL CPreviewPane::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, m_crBg);
	return TRUE;
}

void CPreviewPane::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutChildren();
}
