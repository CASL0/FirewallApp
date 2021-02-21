#include "Firewall.h"
#include "Win32Exception.h"
#include "resource.h"
#include "uiConfigure.h"
#include <Windows.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/debug_output_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#pragma comment(lib, "LibFirewall.lib")

using namespace Win32Util;
using namespace ::WfpUtil;
namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

#define FWM_DISABLE_FORM (WM_APP + 1)
#define FWM_CHECKBOX     (FWM_DISABLE_FORM + 1)
#define FWM_IP_CHECK     (FWM_CHECKBOX + 1)
#define FWM_PORT_CHECK   (FWM_IP_CHECK + 1)

static std::shared_ptr<CFirewall> pFirewall = nullptr;

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

    static HWND hWndList = nullptr;
    static HWND hWndComboAction = nullptr;
    static std::map<std::string, HWND> hWndButton;
    static std::map<std::string, HWND> hWndEdit;
    static std::map<std::string, HWND> hWndCheckBox;
    static HWND hWndTextAllBlock = nullptr;

    static const std::vector<std::string> IpKey = {
        "addr",
        "fqdn",
        "url",
    };

    static const std::vector<std::string> PortKey = {
        "port",
        "protocol",
        "url",
    };

    static bool isAllBlock = false;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        //UI�p�[�c�̐ݒ�

        {
            //�t�B���^�[�\���p���X�g
            hWndList = GetDlgItem(hWndDlg, IDC_LIST);

            //�S�Ւf�p�̃e�L�X�g
            hWndTextAllBlock = GetDlgItem(hWndDlg, IDC_TEXT_ALLBLOCK);
            SetWindowText(hWndTextAllBlock, L"");

            InitButton(hWndDlg  , hWndButton);
            InitEdit(hWndDlg    , hWndEdit);
            InitCheckBox(hWndDlg, hWndCheckBox);
            InitComboBox(hWndDlg, hWndComboAction);

            //������ԂŃ`�F�b�N���
            SendMessage(hWndCheckBox["url"]   , BM_SETCHECK, BST_CHECKED, 0);
            SendMessage(hWndCheckBox["process"], BM_SETCHECK, BST_CHECKED, 0);

            //������ԂŖ�����
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)"addr"    , 0);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)"port"    , 0);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)"fqdn"    , 0);
            SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)"protocol", 0);

        }

        boost::program_options::options_description opt;
        opt.add_options()
            ("trace,t", "");
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(__argc, __wargv, opt), vm);
        boost::program_options::notify(vm);
        if (vm.count("trace"))
        {
            logging::add_common_attributes();
            logging::add_file_log(
                keywords::file_name = "trace.log", // log���o�͂���t�@�C����
                keywords::format    = "%Tag%: [%TimeStamp%] [%ThreadID%] %Message%" // log�̃t�H�[�}�b�g
            );
        }

        try
        {
            pFirewall = std::make_shared<CFirewall>();
        }
        catch (std::runtime_error& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::CFirewall failed with error: " << e.what();
            MessageBox(hWndDlg, L"�t�@�C�A�E�H�[���̏������Ɏ��s���܂���", L"", MB_ICONERROR | MB_OK);
            exit(1);
            break;
        }

        return (INT_PTR)TRUE;
    }
    case WM_CLOSE:
        try
        {
            pFirewall->close();
        }
        catch (std::runtime_error& e)
        {
            BOOST_LOG_TRIVIAL(trace) << "CFirewall::close failed with error: " << e.what();
            MessageBox(hWndDlg, L"�t�@�C�A�E�H�[���̏I�������Ɏ��s���܂���", L"", MB_ICONERROR | MB_OK);
            exit(1);
            break;
        }
        EndDialog(hWndDlg, 0);
        return (INT_PTR)TRUE;
    case FWM_IP_CHECK:
    {
        LPCSTR sKey = (LPCSTR)wParam;
        for (const auto& elem : IpKey)
        {
            if (sKey != elem)
            {
                SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)elem.c_str(), 0);
            }
        }
        return (INT_PTR)TRUE;
    }
    case FWM_PORT_CHECK:
    {
        LPCSTR sKey = (LPCSTR)wParam;
        for (const auto& elem : PortKey)
        {
            if (sKey != elem)
            {
                SendMessage(hWndDlg, FWM_DISABLE_FORM, (WPARAM)elem.c_str(), 0);
            }
        }
        return (INT_PTR)TRUE;
    }
    case FWM_DISABLE_FORM:
    {
        LPCSTR sKeky = (LPCSTR)wParam;
        SendMessage(hWndCheckBox[sKeky], BM_SETCHECK   , BST_UNCHECKED, 0);
        SendMessage(hWndEdit[sKeky]    , EM_SETREADONLY, TRUE         , 0);
        return (INT_PTR)TRUE;
    }
    case FWM_CHECKBOX:
    {
        LPCSTR sKey = (LPCSTR)wParam;
        bool isChecked = BST_CHECKED == SendMessage(hWndCheckBox[sKey], BM_GETCHECK, 0, 0);

        //�`�F�b�N���O�����ꍇ�̓t�H�[���𖳌�������
        if (!isChecked)
        {
            SendMessage(hWndDlg, FWM_DISABLE_FORM, wParam, 0);
            return (INT_PTR)TRUE;
        }

        SendMessage(hWndEdit[sKey], EM_SETREADONLY, FALSE, 0);
        bool isIpCheck = IpKey.end() != std::find(IpKey.begin(), IpKey.end(), sKey);
        if (isIpCheck)
        {
            SendMessage(hWndDlg, FWM_IP_CHECK, wParam, 0);
        }

        bool isPortCheck = PortKey.end() != std::find(PortKey.begin(), PortKey.end(), sKey);
        if (isPortCheck)
        {
            SendMessage(hWndDlg, FWM_PORT_CHECK, wParam, 0);
        }
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_ADD:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndButton["add"], FALSE);

            std::wstringstream ssListItem;
            bool isPermit;
            try
            {
                for (const auto& elem : hWndCheckBox)
                {
                    if (SendMessage(elem.second, BM_GETCHECK, 0, 0) != BST_CHECKED)
                    {
                        continue;
                    }

                    constexpr int BUFFER_LENGTH = 1024;
                    std::vector<CHAR> sEditBuffer(BUFFER_LENGTH);
                    GetWindowTextA(hWndEdit[elem.first], sEditBuffer.data(), BUFFER_LENGTH);
                    
                    if (elem.first == "addr")
                    {
                        pFirewall->AddIpAddrCondition(sEditBuffer.data());
                    }
                    else if (elem.first == "port")
                    {
                        pFirewall->AddPortCondition(std::atoi(sEditBuffer.data()));
                    }
                    else if (elem.first == "fqdn")
                    {
                        pFirewall->AddFqdnCondition(sEditBuffer.data());
                    }
                    else if (elem.first == "protocol")
                    {
                        pFirewall->AddPortCondition(sEditBuffer.data());
                    }
                    else if (elem.first == "url")
                    {
                        pFirewall->AddUrlCondition(sEditBuffer.data());
                    }
                    else if (elem.first == "process")
                    {
                        pFirewall->AddProcessCondition(sEditBuffer.data());
                    }
                    ssListItem << sEditBuffer.data() << ", ";
                }
                isPermit = 0 == (int)SendMessage(hWndComboAction, CB_GETCURSEL, 0, 0);
                pFirewall->AddFilter(isPermit ? FW_ACTION_PERMIT : FW_ACTION_BLOCK);
            }
            catch (std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AddFilter failed with error: " << e.what();
                MessageBox(hWndDlg, L"�t�B���^�[�̒ǉ��Ɏ��s���܂���", L"", MB_ICONERROR | MB_OK);
                EnableWindow(hWndButton["add"], TRUE);
                break;
            }

            ssListItem << (isPermit ? L"����" : L"�Ւf");

            //ListBox�̖����ɒǉ�(��3������-1���w��)
            SendMessage(hWndList, LB_INSERTSTRING, -1, (LPARAM)ssListItem.str().c_str());

            for (const auto& elem : hWndEdit)
            {
                SetWindowText(elem.second, L"");
            }
            EnableWindow(hWndButton["add"], TRUE);
            return (INT_PTR)TRUE;
        }
        case IDC_BUTTON_DEL:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndButton["del"], FALSE);

            //���I���̏ꍇ��-1���Ԃ��Ă���
            LRESULT idx = SendMessage(hWndList, LB_GETCURSEL, 0, 0);
            if (idx == -1)
            {
                EnableWindow(hWndButton["del"], TRUE);
                return (INT_PTR)TRUE;
            }
            int id = MessageBox(hWndDlg, L"�폜���܂����H", L"", MB_OKCANCEL | MB_ICONEXCLAMATION);

            if (id == IDCANCEL)
            {
                EnableWindow(hWndButton["del"], TRUE);
                return (INT_PTR)TRUE;
            }

            try
            {
                pFirewall->RemoveFilter(idx);
            }
            catch (std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::RemovingFilter failed with error: " << e.what();
                MessageBox(hWndDlg, L"�t�B���^�[�̍폜�Ɏ��s���܂���", L"", MB_ICONERROR | MB_OK);
                EnableWindow(hWndButton["del"], TRUE);
                break;
            }
            SendMessage(hWndList, LB_DELETESTRING, idx, 0);
            EnableWindow(hWndButton["del"], TRUE);
            return (INT_PTR)TRUE;        
        }
        case IDC_BUTTON_ALLBLOCK:
        {
            //��d������h�����ߖ��������Ă���
            EnableWindow(hWndButton["allBlock"], FALSE);
            try
            {
                pFirewall->AllBlock(!isAllBlock, FW_DIRECTION_OUTBOUND);
            }
            catch (const std::runtime_error& e)
            {
                BOOST_LOG_TRIVIAL(trace) << "CFirewall::AllBlock failed with error: " << e.what();
                MessageBox(hWndDlg, L"�t�B���^�[�̒ǉ��Ɏ��s���܂���", L"", MB_ICONERROR | MB_OK);
                EnableWindow(hWndButton["allBlock"], TRUE);
                break;
            }
            isAllBlock = !isAllBlock;
            SetWindowText(hWndButton["allBlock"], isAllBlock ? L"�S�Ւf�̉���" : L"�S�Ւf�̓K�p");
            SetWindowText(hWndTextAllBlock, isAllBlock ? L"�S�ʐM���Ւf���I" : L"");
            EnableWindow(hWndButton["allBlock"], TRUE);
            return (INT_PTR)TRUE;

        }
        case IDC_CHECK_ADDR:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)"addr", 0);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_FQDN:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)"fqdn", 0);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PORT:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)"port", 0);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PROTOCOL:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)"protocol", 0);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_URL:
        {
            SendMessage(hWndDlg, FWM_CHECKBOX, (WPARAM)"url", 0);
            return (INT_PTR)TRUE;
        }
        case IDC_CHECK_PROCESS:
        {
            bool isChecked = BST_CHECKED == SendMessage(hWndCheckBox["process"], BM_GETCHECK, 0, 0);

            //�`�F�b�N���O�����ꍇ�t�H�[���𖳌�������
            SendMessage(hWndEdit["process"], EM_SETREADONLY, !isChecked, 0);
            
            return (INT_PTR)TRUE;
        }
        }   //switch (LOWORD(wParam))
        break;
    }
    return (INT_PTR)FALSE;
}