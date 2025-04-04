
// WinSmoovDlg.h : header file
//

#pragma once

#include <initguid.h>
#include "WindowsAudioInterface.h"

// CWinSmoovDlg dialog
class CWinSmoovDlg : public CDialogEx
{
// Construction
public:
	CWinSmoovDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_WINSMOOV_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;
	CComboBox* comboBoxAudioInputs;
	CComboBox* comboBoxAudioOutputs;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnCbnSelchangeComboAudioInputDevice();
	afx_msg void OnCbnSelchangeComboAudioOutputDevice();
private:
	WindowsAudioInterface* winAudioInterface;
};
