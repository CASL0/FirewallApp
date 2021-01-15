#include "Firewall.h"
#include "Win32Exception.h"
#include "resource.h"
#include <Windows.h>
#include <sstream>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/debug_output_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>

#pragma comment(lib, "LibFirewall.lib")

using namespace Win32Util;
using namespace ::WfpUtil;
namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

static const std::wstring STRING_BTN_ADD = L"�ǉ�";
static const std::wstring STRING_BTN_DEL = L"�폜";
static const std::wstring STRING_COMBO[] = { L"����",L"�Ւf" };
static const std::wstring STRING_TEXT_ADDR = L"IP�A�h���X";
static const std::wstring STRING_TEXT_PROTOCOL = L"�v���g�R��";
static const std::wstring STRING_TEXT_ACTION = L"�A�N�V����";
static DWORD INIT_COMBO_SEL = 0;
static const DWORD LENGTH_BUFFER = 1024;

static std::shared_ptr<CFirewall> pFirewall = std::make_shared<CFirewall>();

INT_PTR CALLBACK DialogFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), HWND_DESKTOP, (DLGPROC)DialogFunc);
    return 0;
}


INT_PTR CALLBACK DialogFunc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    static HWND hWndButtonAdd = nullptr;
    static HWND hWndButtonDel = nullptr;
    static HWND hWndEditAddr = nullptr;
    static HWND hWndEditProtocol = nullptr;
    static HWND hWndList = nullptr;
    static HWND hWndComboAction = nullptr;
    static HWND hWndTextAddr = nullptr;
    static HWND hWndTextProtocol = nullptr;
    static HWND hWndTextAction = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:

        //UI�p�[�c�̐ݒ�

        //�u�ǉ��v�{�^��
        hWndButtonAdd = GetDlgItem(hWndDlg, IDC_BUTTON_ADD);
        SetWindowText(hWndButtonAdd, STRING_BTN_ADD.c_str());

        //�u�폜�v�{�^��
        hWndButtonDel = GetDlgItem(hWndDlg, IDC_BUTTON_DEL);
        SetWindowText(hWndButtonDel, STRING_BTN_DEL.c_str());

        //IP�A�h���X���̓t�H�[��
        hWndEditAddr = GetDlgItem(hWndDlg, IDC_IPADDRESS);

        //�v���g�R�����̓t�H�[��
        hWndEditProtocol = GetDlgItem(hWndDlg, IDC_EDIT_PROTOCOL);

        //�t�B���^�[�\���p���X�g
        hWndList = GetDlgItem(hWndDlg, IDC_LIST);

        //�A�N�V�����R���{�{�b�N�X
        hWndComboAction = GetDlgItem(hWndDlg, IDC_COMBO);
        SendMessage(hWndComboAction, CB_ADDSTRING, 0, (LPARAM)STRING_COMBO[0].c_str());
        SendMessage(hWndComboAction, CB_ADDSTRING, 0, (LPARAM)STRING_COMBO[1].c_str());
        SendMessage(hWndComboAction, CB_SETCURSEL, INIT_COMBO_SEL, 0);

        //�X�^�e�B�b�N�e�L�X�g��
        SetWindowText(hWndTextAddr, STRING_TEXT_ADDR.c_str());
        SetWindowText(hWndTextProtocol, STRING_TEXT_PROTOCOL.c_str());
        SetWindowText(hWndTextAction, STRING_TEXT_ACTION.c_str());

        logging::add_common_attributes();
        logging::add_file_log(
            keywords::file_name = "firewall.log", // log���o�͂���t�@�C����
            keywords::format =
            "%Tag%: [%TimeStamp%] [%ThreadID%] %Message%" // log�̃t�H�[�}�b�g
        );
        return (INT_PTR)TRUE;
    case WM_CLOSE:
        try
        {
            pFirewall->close();
        }
        //HRESULT�̕ߑ�
        catch (CWin32Exception<HRESULT>& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::close failed";
            break;
        }

        //WSAGetLastError()�̕ߑ�
        catch (CWin32Exception<int>& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::close failed";
            break;
        }
        EndDialog(hWndDlg, 0);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_ADD:
        {
            std::vector<CHAR> sIpAddr(LENGTH_BUFFER);
            std::vector<CHAR> sProtocol(LENGTH_BUFFER);
            GetWindowTextA(hWndEditAddr, sIpAddr.data(), LENGTH_BUFFER);
            GetWindowTextA(hWndEditProtocol, sProtocol.data(), LENGTH_BUFFER);
            int iCurSel = (int)SendMessage(hWndComboAction, CB_GETCURSEL, 0, 0);

            try
            {
                pFirewall->AddFilter(iCurSel == 0 ? WFP_ACTION_PERMIT : WFP_ACTION_BLOCK, sIpAddr.data(), sProtocol.data());
            }

            //GetLastError()�̕ߑ�
            catch (CWin32Exception<DWORD>& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AddFilter failed";
                break;
            }

            //HRESULT�̕ߑ�
            catch (CWin32Exception<HRESULT>& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AddFilter failed";
                break;
            }

            //WSAGetLastError()�̕ߑ�
            catch (CWin32Exception<int>& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AddFilter failed";
                break;
            }
            std::wstringstream ssListItem;
            ssListItem << sIpAddr.data() << L"    " << sProtocol.data() << L"    " << STRING_COMBO[iCurSel];

            SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)ssListItem.str().c_str());

            SetWindowText(hWndEditAddr, L"");
            SetWindowText(hWndEditProtocol, L"");
            return (INT_PTR)TRUE;
        }
        case IDC_BUTTON_DEL:
        {
            //���I���̏ꍇ��-1���Ԃ��Ă���
            int idx = SendMessage(hWndList, LB_GETCURSEL, 0, 0);
            if (idx == -1)
            {
                return (INT_PTR)TRUE;
            }
            int id = MessageBox(hWndDlg, L"�폜���܂����H", L"", MB_OKCANCEL | MB_ICONEXCLAMATION);

            if (id == IDCANCEL)
            {
                return (INT_PTR)TRUE;
            }
            SendMessage(hWndList, LB_DELETESTRING, idx, 0);
            return (INT_PTR)TRUE;        
        }
        }   //switch (LOWORD(wParam))
        break;
    }
    return (INT_PTR)FALSE;
}