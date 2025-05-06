#pragma once

#ifdef _WIN32
// 구조체가 다룰 수 있는 최대 소켓(파일 디스크립터) 개수를 지정
// Windows 환경에선 기본 64보다 5096으로 늘려서 더 많은 소켓 다룰 수 있도록 설정
// Linux/Unix 에서는 기본 1024까지만 지원
#define FD_SETSIZE 5096
// 윈도우 소켓 라이브러리 ws2_32.lib를 자동으로 링크
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
// Linux는 그냥 int로 소켓 디스크립터로 나타냄으로 호환용 코드
#define SOCKET int
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#include <vector>
#include <deque>
#include "interface_tcp_network.h"

namespace NServerNetLib
{
	class TcpNetwork : public ITcpNetwork
	{
	public:
		TcpNetwork();
		virtual ~TcpNetwork();

		NET_ERROR_CODE Init(const ServerConfig* pConfig, ILog* pLogger) override;

		NET_ERROR_CODE SendData(const int32_t sessionIndex, const int16_t packetId,
			const int16_t bodySize, const char* pMsg) override;

		void Run() override;

		void Release() override;

		void ForcingClose(const int32_t sessionIndex) override;

		RecvPacketInfo GetPacketInfo() override;

		int32_t ClientSessionPoolSize() override { return (int32_t)m_ClientSessionPool.size(); }		

	// 메서드 구역
	protected:
		NET_ERROR_CODE InitServerSocket();
		NET_ERROR_CODE BindListen(int16_t port, int32_t backlogCount);
		NET_ERROR_CODE SetNonBlockSocket(const SOCKET sock);

		int32_t AllocClientSessionIndex();
		void ReleaseSessionIndex(const int32_t index);

		int32_t CreateSessionPool(const int32_t maxClientCount);
		NET_ERROR_CODE NewSession();
		void SetSockOption(const SOCKET fd);
		void ConnectedSession(const int32_t sessionIndex, const SOCKET fd, const char* pIP);

		void CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int32_t sessionIndex);

		NET_ERROR_CODE RecvSocket(const int32_t sessionIndex);
		NET_ERROR_CODE RecvBufferProcess(const int32_t sessionIndex);
		void AddPacketQueue(const int32_t sessionIndex, const int16_t pktId, const int16_t bodySize, int8_t* pDataPos);

		void RunProcessWrite(const int32_t sessionIndex, const SOCKET fd, fd_set& write_set);
		NetError FlushSendBuff(const int32_t sessionIndex);
		NetError SendSocket(const SOCKET fd, const char* pMsg, const int32_t size);

		bool RunCheckSelectResult(const int32_t result);
		void RunCheckSelectClients(fd_set& read_set, fd_set& wrtie_set);
		bool RunProcessReceive(const int32_t sessionIndex, const SOCKET fd, fd_set& read_set);

	// 변수 구역
	protected:
		ServerConfig m_Config;

		SOCKET m_ServerSockFD;
		// 가장 큰 소켓 디스크립터 값
		SOCKET m_MaxSockFD = 0;

		fd_set m_Readfds;
		size_t m_ConnectedSessionCount = 0;

		int64_t m_ConnectSeq = 0;

		std::vector<ClientSession>m_ClientSessionPool;
		std::deque<int> m_ClientSessionPoolIndex;

		std::deque<RecvPacketInfo> m_PacketQueue;

		ILog* m_pRefLogger;
	};


}