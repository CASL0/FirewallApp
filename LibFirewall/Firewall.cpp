#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <WinInet.h>
#include <vector>
#include <fwpmu.h>
#include <memory>
#include <array>
#include <boost/log/trivial.hpp>

#include "Firewall.h"
#include "Win32Exception.h"

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

#pragma warning(disable : 4996)

namespace Win32Util{ namespace WfpUtil{
	typedef struct
	{
		UINT16 port;		//�|�[�g�ԍ�
		UINT32 mask;		//�T�u�l�b�g�}�X�N
		UINT32 hexAddr;		//IP�A�h���X
		UINT64 filterID;	//�t�B���^�[ID

	} FILTER_COND_INFO;

	typedef struct
	{
		UINT64 v4;
		UINT64 v6;
	} FILTER_ID;

	typedef struct _IP_ADDR
	{
		int af;
		union
		{
			UINT32 v4;
			std::array<BYTE, 16> v6;
		};
	} IP_ADDR;

	class CFirewall::Impl
	{
	public:
		HANDLE m_hEngine;
		GUID m_subLayerGUID;
		std::vector<FILTER_ID> m_vecFilterId;
		std::vector<FWPM_FILTER_CONDITION0> m_vecConditions;
		std::vector<IP_ADDR> m_vecIpAddr;
		BYTE m_flagConditions;
	public:
		Impl();
		~Impl() = default;
		void close();

		void AddIpAddrCondition(const std::string& sIpAddr);
		void AddIpAddrCondition(const std::string& sIpAddr, UINT32 dwMask);
		void AddPortCondition(UINT16 wPort);
		void AddPortCondition(const std::string& sProtocol);
		void AddFqdnCondition(const std::string& sFqdn);
		void AddUrlCondition(const std::string& sUrl);
		void AddProcessCondition(const std::string& sPathToApp);
		void AddFilter(FW_ACTION action);
		
		//�t�B���^�[�̍��Ԃ��w�肵�č폜����
		//���Ԗڂɒǉ����������w�肷��(0-based)
		//��FRemoveFilter(2) -> �R�Ԗڂɒǉ������t�B���^�[���폜����
		void RemoveFilter(int index);

		void WfpSetup();
		void AddSubLayer();
		void RemoveSubLayer();
		void RemoveAllFilters();

		//[in] sUrl: URL
		//[out] sFqdn: FQDN, sProtocol: �v���g�R��
		//�p�[�X�Ɏ��s�����ꍇWin32Exception��throw����(�N���C�A���g�R�[�h�ł͗�O��ߑ�����)
		void ParseUrl(const std::string& sUrl, std::string& sFqdn, std::string& sProtocol);

		//�z�X�g������������
		//���݂��Ȃ��ꍇWin32Exception��throw����(�N���C�A���g�R�[�h�ł͗�O��ߑ�����)
		void FqdnToIpAddr(const std::string& sFqdn);

		//�T�[�r�X������������(etc/services����̉���)
		//���݂��Ȃ��ꍇruntime_error��throw����(�N���C�A���g�R�[�h�ł͗�O��ߑ�����)
		UINT16 GetPortByServ(const std::string& sService);

		//IP�A�h���X�̕����񂩂�z�X�g�I�[�_�[�֕ϊ�
		//���͗�F"192.168.0.1"
		UINT32 TranslateStr2Hex(const std::string& sAddr);

		std::wstring AstrToWstr(const std::string& src);

		inline void SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, UINT32 dwAddr, UINT32 dwMask);
		inline void SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, UINT16 wPort);
		inline void SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, const std::string& sPathToApp);
	};

	CFirewall::Impl::Impl() :
		m_hEngine(nullptr),
		m_subLayerGUID({ 0 }),
		m_vecFilterId(std::vector<FILTER_ID>()), 
		m_vecConditions(std::vector<FWPM_FILTER_CONDITION0>()),
		m_vecIpAddr(std::vector<IP_ADDR>()),
		m_flagConditions(0)
	{
		DWORD dwRet;
		WSADATA wsaData;
		dwRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		ThrowWsaError(dwRet != 0, "WSAStartup failed");
		WfpSetup();
	}

	void CFirewall::Impl::close()
	{
		RemoveAllFilters();

		DWORD dwRet;
		dwRet = WSACleanup();
		ThrowWsaError(dwRet != 0, "WSACleanup failed");

		RemoveSubLayer();
		dwRet = FwpmEngineClose0(m_hEngine);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmEngineClose0 failed");
	}

	void CFirewall::Impl::WfpSetup()
	{
		BOOST_LOG_TRIVIAL(trace) << "WfpSetup begins";
		DWORD dwRet;
		dwRet = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, nullptr, &m_hEngine);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmEngineOpen0 failed");
		AddSubLayer();
	}

	void CFirewall::Impl::AddSubLayer()
	{
		BOOST_LOG_TRIVIAL(trace) << "AddSubLayer begins";
		FWPM_SUBLAYER0 fwpSubLayer = { 0 };
		RPC_STATUS rpcStatus = RPC_S_OK;

		rpcStatus = UuidCreate(&fwpSubLayer.subLayerKey);
		ThrowLastError(rpcStatus != RPC_S_OK, "UuidCreate failed");
		CopyMemory(&m_subLayerGUID, &fwpSubLayer.subLayerKey, sizeof(fwpSubLayer.subLayerKey));
		BOOST_LOG_TRIVIAL(trace) << "UuidCreate succeeded";

		fwpSubLayer.displayData.name = const_cast<WCHAR*>(L"WfpSublayer");
		fwpSubLayer.displayData.description = const_cast<WCHAR*>(L"create WfpSublayer");
		fwpSubLayer.flags = 0;
		fwpSubLayer.weight = 0x100;

		BOOST_LOG_TRIVIAL(trace) << "Adding Sublayer";
		DWORD dwRet = FwpmSubLayerAdd0(m_hEngine, &fwpSubLayer, nullptr);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmSubLayerAdd0 failed");
	}

	void CFirewall::Impl::RemoveSubLayer()
	{
		BOOST_LOG_TRIVIAL(trace) << "Removing Sublayer";
		DWORD dwRet = FwpmSubLayerDeleteByKey0(m_hEngine, &m_subLayerGUID);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmSubLayerDeleteByKey0 failed");
		ZeroMemory(&m_subLayerGUID, sizeof(GUID));
	}

	UINT32 CFirewall::Impl::TranslateStr2Hex(const std::string& sAddr)
	{
		in_addr hexAddr;
		int iRet = inet_pton(AF_INET, sAddr.c_str(), &hexAddr);
		ThrowWsaError(iRet != 1, "inet_pton failed");
		return ntohl(hexAddr.S_un.S_addr);
	}

	inline void CFirewall::Impl::SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, UINT32 dwAddr, UINT32 dwMask)
	{
		std::shared_ptr<FWP_V4_ADDR_AND_MASK> pFwpAddrMask = std::make_shared<FWP_V4_ADDR_AND_MASK>();
		pFwpAddrMask->addr = dwAddr;
		pFwpAddrMask->mask = dwMask;

		FWPM_FILTER_CONDITION0 fwpCondition = { 0 };
		fwpCondition.fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
		fwpCondition.matchType = FWP_MATCH_EQUAL;
		fwpCondition.conditionValue.type = FWP_V4_ADDR_MASK;
		fwpCondition.conditionValue.v4AddrMask = pFwpAddrMask.get();
		vecFwpConditions.push_back(fwpCondition);
	}

	inline void CFirewall::Impl::SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, UINT16 wPort)
	{
		FWPM_FILTER_CONDITION0 fwpCondition = { 0 };
		fwpCondition.fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
		fwpCondition.matchType = FWP_MATCH_EQUAL;
		fwpCondition.conditionValue.type = FWP_UINT16;
		fwpCondition.conditionValue.uint16 = wPort;
		vecFwpConditions.push_back(fwpCondition);
	}

	inline void CFirewall::Impl::SetupConditions(std::vector<FWPM_FILTER_CONDITION0>& vecFwpConditions, const std::string& sPathToApp)
	{
		FWP_BYTE_BLOB* appBlob = nullptr;
		DWORD dwRet = FwpmGetAppIdFromFileName0(AstrToWstr(sPathToApp).c_str(), &appBlob);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmGetAppIdFromFileName0 failed");
		FWPM_FILTER_CONDITION0 fwpCondition = { 0 };
		fwpCondition.fieldKey = FWPM_CONDITION_ALE_APP_ID;
		fwpCondition.matchType = FWP_MATCH_EQUAL;
		fwpCondition.conditionValue.type = FWP_BYTE_BLOB_TYPE;
		fwpCondition.conditionValue.byteBlob = appBlob;
		vecFwpConditions.push_back(fwpCondition);
		FwpmFreeMemory0((void**)&appBlob);
	}

	std::wstring CFirewall::Impl::AstrToWstr(const std::string& src)
	{
		auto destSize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src.c_str(), -1, nullptr, 0);
		std::vector<WCHAR> dest(destSize);
		destSize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src.c_str(), -1, dest.data(), dest.size());
		ThrowLastError(destSize == 0, "MultiByteToWideChar failed");
		dest.resize(destSize);
		return std::wstring(dest.begin(), dest.end());
	}

	void CFirewall::Impl::RemoveAllFilters()
	{
		BOOST_LOG_TRIVIAL(trace) << "RemoveAllFilters begins";
		DWORD dwRet = ERROR_BAD_COMMAND;
		for (auto& elem : m_vecFilterId)
		{
			BOOST_LOG_TRIVIAL(trace) << "Removing filter";
			dwRet = FwpmFilterDeleteById0(m_hEngine, elem.v4);
			ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterDeleteById0 failed");

			dwRet = FwpmFilterDeleteById0(m_hEngine, elem.v6);
			ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterDeleteById0 failed");
		}
	}

	void CFirewall::Impl::ParseUrl(const std::string& sUrl, std::string& sFqdn, std::string& sProtocol)
	{
		BOOST_LOG_TRIVIAL(trace) << "URL: " << sUrl;
		URL_COMPONENTSA urlComponents = { 0 };

		static const int URL_BUFFER_SIZE = 1024;
		std::vector<CHAR> host(URL_BUFFER_SIZE);
		std::vector<CHAR> scheme(URL_BUFFER_SIZE);

		urlComponents.dwStructSize     = sizeof(URL_COMPONENTSA);
		urlComponents.lpszHostName     = host.data();
		urlComponents.lpszScheme       = scheme.data();
		urlComponents.dwHostNameLength = URL_BUFFER_SIZE;
		urlComponents.dwSchemeLength   = URL_BUFFER_SIZE;

		bool bRet = InternetCrackUrlA(sUrl.c_str(), sUrl.length(), 0, &urlComponents);
		ThrowLastError(bRet == false, "InternetCrackUrl failed");

		BOOST_LOG_TRIVIAL(trace) << "InternetCrackUrl succeeded";
		host.resize(urlComponents.dwHostNameLength);
		scheme.resize(urlComponents.dwSchemeLength);

		sFqdn     = std::string(host.begin(), host.end());
		sProtocol = std::string(scheme.begin(), scheme.end());
	}

	void CFirewall::Impl::FqdnToIpAddr(const std::string& sFqdn)
	{
		BOOST_LOG_TRIVIAL(trace) << "FQDN: " << sFqdn;
		addrinfo hints = { 0 };
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_ALL;

		addrinfo* pAddrInfo = nullptr;
		int iRet = getaddrinfo(sFqdn.c_str(), nullptr, &hints, &pAddrInfo);
		ThrowWsaError(iRet != 0, "getaddrinfo failed");

		BOOST_LOG_TRIVIAL(trace) << "getaddrinfo succeeded";

		SOCKADDR_IN*  sockAddrV4 = nullptr;
		SOCKADDR_IN6* sockAddrV6 = nullptr;
		for (ADDRINFO* ptr = pAddrInfo; ptr != nullptr; ptr->ai_next)
		{
			IP_ADDR ipAddr = { 0 };
			switch (ptr->ai_family)
			{
			case AF_INET:
			{
				sockAddrV4 = (SOCKADDR_IN*)ptr->ai_addr;
				ipAddr.af = AF_INET;
				ipAddr.v4 = ntohl(sockAddrV4->sin_addr.S_un.S_addr);
				break;
			}
			case AF_INET6:
			{
				sockAddrV6 = (SOCKADDR_IN6*)ptr->ai_addr;
				ipAddr.af = AF_INET6;
				CopyMemory(ipAddr.v6.data(), sockAddrV6->sin6_addr.u.Byte, 16);
				break;
			}
			default:
				continue;
			}
			m_vecIpAddr.push_back(ipAddr);
		}

		freeaddrinfo(pAddrInfo);
	}

	UINT16 CFirewall::Impl::GetPortByServ(const std::string& sService)
	{
		servent* pServEnt = getservbyname(sService.c_str(), nullptr);
		if (pServEnt == nullptr)
		{
			throw std::runtime_error("protocol not found");
		}
		return ntohs(pServEnt->s_port);
	}

	void CFirewall::Impl::RemoveFilter(int index)
	{
		BOOST_LOG_TRIVIAL(trace) << "RemoveFilter begins";
		DWORD dwRet = ERROR_BAD_COMMAND;

		BOOST_LOG_TRIVIAL(trace) << "Removing filter";
		dwRet = FwpmFilterDeleteById0(m_hEngine, m_vecFilterId.at(index).v4);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterDeleteById0 failed");
		dwRet = FwpmFilterDeleteById0(m_hEngine, m_vecFilterId.at(index).v6);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterDeleteById0 failed");
		m_vecFilterId.erase(m_vecFilterId.cbegin() + index);
	}

	void CFirewall::Impl::AddIpAddrCondition(const std::string& sIpAddr, UINT32 dwMask)
	{
		UINT32 hexAddr = TranslateStr2Hex(sIpAddr);
		SetupConditions(m_vecConditions, hexAddr, dwMask);
		BOOST_LOG_TRIVIAL(trace) << "Adding a condition: " << sIpAddr;
	}

	void CFirewall::Impl::AddIpAddrCondition(const std::string& sIpAddr)
	{
		AddIpAddrCondition(sIpAddr, 0xffffffff);
	}

	void CFirewall::Impl::AddPortCondition(UINT16 wPort)
	{
		SetupConditions(m_vecConditions, wPort);
		BOOST_LOG_TRIVIAL(trace) << "Adding a condition: " << wPort;
	}

	void CFirewall::Impl::AddPortCondition(const std::string& sProtocol)
	{
		UINT16 wPort = GetPortByServ(sProtocol);
		AddPortCondition(wPort);
	}

	void CFirewall::Impl::AddFqdnCondition(const std::string& sFqdn)
	{
		FqdnToIpAddr(sFqdn);
		AddIpAddrCondition(sIpAddr);
	}

	void CFirewall::Impl::AddUrlCondition(const std::string& sUrl)
	{
		std::string sFqdn;
		std::string sProtocol;
		ParseUrl(sUrl, sFqdn, sProtocol);

		AddFqdnCondition(sFqdn);
		AddPortCondition(sProtocol);
	}

	void CFirewall::Impl::AddProcessCondition(const std::string& sPathToApp)
	{
		BOOST_LOG_TRIVIAL(trace) << "App: " << sPathToApp;
		SetupConditions(m_vecConditions, sPathToApp);
		BOOST_LOG_TRIVIAL(trace) << "Adding a condition: " << sPathToApp;
	}

	void CFirewall::Impl::AddFilter(FW_ACTION action)
	{
		BOOST_LOG_TRIVIAL(trace) << "AddFilter begins";
		FWPM_FILTER0 fwpFilter = { 0 };
		fwpFilter.subLayerKey = m_subLayerGUID;

		fwpFilter.weight.type = FWP_EMPTY;

		fwpFilter.displayData.name = const_cast<WCHAR*>(L"IPv4Permit");
		fwpFilter.displayData.description = const_cast<WCHAR*>(L"Filter for IPv4");

		fwpFilter.action.type = action == FW_ACTION_PERMIT ? FWP_ACTION_PERMIT : FWP_ACTION_BLOCK;

		fwpFilter.numFilterConditions = m_vecConditions.size();
		fwpFilter.filterCondition = m_vecConditions.data();

		BOOST_LOG_TRIVIAL(trace) << "Adding filter";
		FILTER_ID filterId = { 0 };

		//v4�p�̃t�B���^�[��ǉ�
		fwpFilter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;
		DWORD dwRet = FwpmFilterAdd0(m_hEngine, &fwpFilter, nullptr, &filterId.v4);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterAdd0 failed");

		//v6�p�̃t�B���^�[��ǉ�
		fwpFilter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V6;
		dwRet = FwpmFilterAdd0(m_hEngine, &fwpFilter, nullptr, &filterId.v6);
		ThrowHresultError(dwRet != ERROR_SUCCESS, "FwpmFilterAdd0 failed");
		m_vecFilterId.push_back(filterId);
		m_vecConditions.clear();
	}

	CFirewall::CFirewall(): pimpl(std::make_shared<Impl>())
	{
	}

	void CFirewall::close()
	{
		pimpl->close();
	}

	void CFirewall::RemoveFilter(int index)
	{
		pimpl->RemoveFilter(index);
	}

	void CFirewall::AddIpAddrCondition(const std::string& sIpAddr, UINT32 dwMask)
	{
		pimpl->AddIpAddrCondition(sIpAddr, dwMask);
	}

	void CFirewall::AddIpAddrCondition(const std::string& sIpAddr)
	{
		pimpl->AddIpAddrCondition(sIpAddr);
	}

	void CFirewall::AddPortCondition(UINT16 wPort)
	{
		pimpl->AddPortCondition(wPort);
	}

	void CFirewall::AddPortCondition(const std::string& sProtocol)
	{
		pimpl->AddPortCondition(sProtocol);
	}

	void CFirewall::AddFqdnCondition(const std::string& sFqdn)
	{
		pimpl->AddFqdnCondition(sFqdn);
	}

	void CFirewall::AddUrlCondition(const std::string& sUrl)
	{
		pimpl->AddUrlCondition(sUrl);
	}

	void CFirewall::AddProcessCondition(const std::string& sPathToApp)
	{
		pimpl->AddProcessCondition(sPathToApp);
	}

	void CFirewall::AddFilter(FW_ACTION action)
	{
		pimpl->AddFilter(action);
	}

}	//namespace WfpUtil
}	//namespace Win32Util