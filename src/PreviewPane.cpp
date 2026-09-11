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

BEGIN_MESSAGE_MAP(CPreviewPane, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CPreviewPane::CPreviewPane() :
	m_pDpi(NULL),
	m_clipId(-1),
	m_bHasMetadata(false),
	m_nTotalSize(0),
	m_bTruncatedText(false),
	m_pBitmap(NULL),
	m_bFileListTruncated(false),
	m_crBg(RGB(255, 255, 255)),
	m_crText(RGB(0, 0, 0)),
	m_crHeaderText(RGB(64, 64, 64))
{
}

CPreviewPane::~CPreviewPane()
{
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

	return TRUE;
}

void CPreviewPane::SetColors(COLORREF bg, COLORREF text, COLORREF headerText)
{
	m_crBg = bg;
	m_crText = text;
	m_crHeaderText = headerText;
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
	m_csImageFormatName.Empty();

	if (clipId > 0)
	{
		LoadMetadata(clipId);
		LoadTextPreview(clipId);
		LoadFileList(clipId);
		// image loaded in LoadMetadata after we know which image format exists
	}

	Invalidate(FALSE);
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

void CPreviewPane::DrawSectionTitle(CDC& dc, CRect& rc, const CString& csTitle)
{
	if (rc.Height() <= 0)
	{
		return;
	}

	CRect rcTitle = rc;
	rcTitle.bottom = rcTitle.top + (m_pDpi ? m_pDpi->Scale(16) : 16);

	dc.SetTextColor(m_crHeaderText);
	dc.DrawText(csTitle, rcTitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

	rc.top += (m_pDpi ? m_pDpi->Scale(20) : 20);
}

void CPreviewPane::DrawBodyText(CDC& dc, CRect& rc, const CString& csText)
{
	if (rc.Height() <= 0 || csText.IsEmpty())
	{
		return;
	}

	dc.SetTextColor(m_crText);
	CRect rcText = rc;
	int nHeight = dc.DrawText(csText, rcText, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX | DT_CALCRECT);
	rcText.bottom = min(rcText.top + nHeight, rc.bottom);
	dc.DrawText(csText, rcText, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);

	rc.top = rcText.bottom + (m_pDpi ? m_pDpi->Scale(4) : 4);
}

void CPreviewPane::OnPaint()
{
	CPaintDC dc(this);

	CRect rcClient;
	GetClientRect(rcClient);

	CFont* pOldFont = NULL;
	CFont* pGuiFont = (CFont*)CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
	if (pGuiFont)
	{
		pOldFont = dc.SelectObject(pGuiFont);
	}

	int oldBkMode = dc.SetBkMode(TRANSPARENT);

	int margin = m_pDpi ? m_pDpi->Scale(8) : 8;
	CRect rc = rcClient;
	rc.DeflateRect(margin, margin, margin, margin);

	if (m_clipId <= 0)
	{
		dc.SetTextColor(m_crHeaderText);
		dc.DrawText(_T("No item selected"), rc, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

		if (pOldFont)
		{
			dc.SelectObject(pOldFont);
		}
		dc.SetBkMode(oldBkMode);
		return;
	}

	if (m_bHasMetadata)
	{
		CString csHeader;
		csHeader.Format(_T("Clip %d - %s"), m_clipId, FormatByteSize(m_nTotalSize));
		DrawSectionTitle(dc, rc, csHeader);

		CString csFormats;
		for (size_t i = 0; i < m_formats.size(); i++)
		{
			CString csLine;
			csLine.Format(_T("%s (%s)"), m_formats[i].m_csName, FormatByteSize(m_formats[i].m_nSize));
			if (csFormats.IsEmpty() == FALSE)
			{
				csFormats += _T("\r\n");
			}
			csFormats += csLine;
		}
		if (csFormats.IsEmpty() == FALSE)
		{
			DrawSectionTitle(dc, rc, _T("Formats"));
			DrawBodyText(dc, rc, csFormats);
		}
	}

	if (rc.Height() > 0 && m_pBitmap != NULL)
	{
		CRect rcImage = rc;
		DrawSectionTitle(dc, rcImage, _T("Image"));

		int nImgWidth = (int)m_pBitmap->GetWidth();
		int nImgHeight = (int)m_pBitmap->GetHeight();
		if (nImgWidth > 0 && nImgHeight > 0 && rcImage.Width() > 0 && rcImage.Height() > 0)
		{
			double dScale = min((double)rcImage.Width() / nImgWidth, (double)rcImage.Height() / nImgHeight);
			dScale = min(dScale, 1.0);
			int nDrawWidth = max(1, (int)(nImgWidth * dScale));
			int nDrawHeight = max(1, (int)(nImgHeight * dScale));

			CRect rcDraw(rcImage.left, rcImage.top, rcImage.left + nDrawWidth, rcImage.top + nDrawHeight);
			Graphics graphics(dc.GetSafeHdc());
			graphics.DrawImage(m_pBitmap, Gdiplus::Rect(rcDraw.left, rcDraw.top, rcDraw.Width(), rcDraw.Height()));

			rc.top = rcDraw.bottom + (m_pDpi ? m_pDpi->Scale(4) : 4);
		}
	}

	if (rc.Height() > 0 && m_fileNames.GetCount() > 0)
	{
		CString csFiles;
		for (int i = 0; i < m_fileNames.GetCount(); i++)
		{
			if (csFiles.IsEmpty() == FALSE)
			{
				csFiles += _T("\r\n");
			}
			csFiles += m_fileNames[i];
		}
		if (m_bFileListTruncated)
		{
			csFiles += _T("\r\n...");
		}
		DrawSectionTitle(dc, rc, _T("Files"));
		DrawBodyText(dc, rc, csFiles);
	}

	if (rc.Height() > 0 && m_csTextPreview.IsEmpty() == FALSE)
	{
		DrawSectionTitle(dc, rc, _T("Content"));
		DrawBodyText(dc, rc, m_csTextPreview);
		if (m_bTruncatedText)
		{
			DrawBodyText(dc, rc, _T("..."));
		}
	}

	if (pOldFont)
	{
		dc.SelectObject(pOldFont);
	}
	dc.SetBkMode(oldBkMode);
}

BOOL CPreviewPane::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillSolidRect(rc, m_crBg);
	return TRUE;
}
