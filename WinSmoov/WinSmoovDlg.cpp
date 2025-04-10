
// WinSmoovDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "WinSmoov.h"
#include "WinSmoovDlg.h"
#include "afxdialogex.h"

#include <vector>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CWinSmoovDlg dialog



CWinSmoovDlg::CWinSmoovDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_WINSMOOV_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	comboBoxAudioInputs = nullptr;
	comboBoxAudioOutputs = nullptr;
	winAudioInterface = nullptr;
}

void CWinSmoovDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CWinSmoovDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CWinSmoovDlg::OnBnClickedOk)
	ON_CBN_SELCHANGE(IDC_COMBO_AUDIO_INPUT_DEVICE, &CWinSmoovDlg::OnCbnSelchangeComboAudioInputDevice)
	ON_CBN_SELCHANGE(IDC_COMBO_AUDIO_OUTPUT_DEVICE, &CWinSmoovDlg::OnCbnSelchangeComboAudioOutputDevice)
END_MESSAGE_MAP()


// CWinSmoovDlg message handlers

BOOL CWinSmoovDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	//Add extra initialization here
	comboBoxAudioInputs = (CComboBox*)GetDlgItem(IDC_COMBO_AUDIO_INPUT_DEVICE);
	comboBoxAudioOutputs = (CComboBox*)GetDlgItem(IDC_COMBO_AUDIO_OUTPUT_DEVICE);

	winAudioInterface = new WindowsAudioInterface();

	std::vector<std::wstring> input_devices;
	winAudioInterface->getInputDevices(input_devices);

	for (uint32_t i = 0; i < input_devices.size(); i++) {
		comboBoxAudioInputs->AddString(CString(input_devices[i].c_str()));
	}

	std::vector<std::wstring> output_devices;
	winAudioInterface->getOutputDevices(output_devices);

	for (uint32_t i = 0; i < output_devices.size(); i++) {
		comboBoxAudioOutputs->AddString(CString(output_devices[i].c_str()));
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWinSmoovDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWinSmoovDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWinSmoovDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CWinSmoovDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	OutputDebugString(CStringW("OK was clicked.\n"));
	delete winAudioInterface;
	CDialogEx::OnOK();
}

void CWinSmoovDlg::OnCbnSelchangeComboAudioInputDevice()
{
	// TODO: Add your control notification handler code here
}

void CWinSmoovDlg::OnCbnSelchangeComboAudioOutputDevice()
{
	// TODO: Add your control notification handler code here
}