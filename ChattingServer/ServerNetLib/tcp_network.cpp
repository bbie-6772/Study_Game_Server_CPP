



// 윈도우가 아닌 Linux/Unix 플랫폼에서 사용
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#endif

#include "interface_log.h"
#include "tcp_network.h"

namespace NServerNetLib
{
	TcpNetwork::TcpNetwork()
	{
	}

	TcpNetwork::~TcpNetwork()	
	{
		// 접속되어 생성된 클라이언트 내부 포인터 해제
		for (auto& client : m_ClientSessionPool)
		{
			if (client.pRecvBuffer) {
				delete[] client.pRecvBuffer;
			}

			if (client.pSendBuffer) {
				delete[] client.pSendBuffer;
			}
		}
	}

	NET_ERROR_CODE TcpNetwork::Init(const ServerConfig* pConfig, ILog* pLogger)
	{
		// 얕은 복사
		std::memcpy(&m_Config, pConfig, sizeof(ServerConfig));

		m_pRefLogger = pLogger;

		// 초기화 오류 감지
		NET_ERROR_CODE initResult = InitServerSocket();
		if (initResult != NET_ERROR_CODE::kNONE) {
			return initResult;
		}
		// 바인딩 오류 감지
		NET_ERROR_CODE bindListenResult = BindListen(pConfig->Port, pConfig->BackLogCount);
		if (bindListenResult != NET_ERROR_CODE::kNONE) {
			return bindListenResult;
		}

		// 소켓 초기화
		FD_ZERO(&m_Readfds);
		// 특정 소켓(m_ServerSockFD)을 감시 소켓 집합에 추가
		FD_SET(m_ServerSockFD, &m_Readfds);

		// 세션풀 생성
		int32_t sessionPoolSize = CreateSessionPool(pConfig->MaxClientCount + pConfig->ExtraClientCount);

		// __FUNCTION__ : 현재 함수의 이름을 문자열 리터럴로 제공하는 매크로
		m_pRefLogger->WriteLog(LOG_LEVEL::kL_INFO, "%s | 세션 Pool 크기 : %d", __FUNCTION__, sessionPoolSize);

		return NET_ERROR_CODE::kNONE;
	}

	int32_t TcpNetwork::CreateSessionPool(const int32_t maxClientCount)
	{
		for (int32_t i = 0; i < maxClientCount; ++i) {
			ClientSession session;
			session.Clear();
			session.Index = i;
			session.pRecvBuffer = new char[m_Config.MaxClientRecvBufferSize];
			session.pSendBuffer = new char[m_Config.MaxClientSendBufferSize];

			m_ClientSessionPool.push_back(session);
			m_ClientSessionPoolIndex.push_back(session.Index);
		}

		return maxClientCount;
	}

	NET_ERROR_CODE TcpNetwork::InitServerSocket()
	{
#ifdef _WIN32
		// 윈도우 환경 Winsock 라이브러리 초기화
		// Winsock 2.2 버전 요청
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		// Winsock 초기화 함수 호출
		WSAStartup(wVersionRequested, &wsaData);

		// TCP 소켓 생성, IPv4, 스트림 타입(TCP), 프로토콜 TCP 지정
		m_ServerSockFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
		//  socket 함수 호출, IPv4, 스트림 타입, 프로토콜 기본값 0
		m_ServerSockFD = socket(PF_INET, SOCK_STREAM, 0);
#endif
		// 소켓 생성 실패 시 에러 코드 반환 (소켓 디스크립터가 음수면 실패)
		if (m_ServerSockFD < 0) {
			return NET_ERROR_CODE::kSERVER_SOCKET_CREATE_FAIL;
		}

		// 소켓 옵션 설정 - SO_REUSEADDR: 소켓 주소 재사용 허용
		int32_t n = 1;
		// setsockopt를 호출해 m_ServerSockFD 소켓에 SO_REUSEADDR 옵션을 켬
		// (char*)&n: 옵션 값 포인터, sizeof(n): 옵션 값 크기 전달
		if (setsockopt(m_ServerSockFD, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0) {
			return NET_ERROR_CODE::kSERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		// 모든 초기화 성공 시 정상 반환
		return NET_ERROR_CODE::kNONE;
	}

	NET_ERROR_CODE TcpNetwork::BindListen(int16_t port, int32_t backlogCount)
	{
		// 서버 소켓용 주소 정보를 설정
		struct sockaddr_in server_addr;
		// 구조체 초기화
		memset(&server_addr, 0, sizeof(server_addr));
		// IPv4 주소체계
		server_addr.sin_family = AF_INET;
		// IP 주소 지정 (hton?  == 네트워크 바이트 순서(빅엔디안) 전환 / INADDR_ANY == 모든 IP 바인딩)
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);

		// 바인딩 시도 후 실패 여부 확인
		if (bind(m_ServerSockFD, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			return NET_ERROR_CODE::kSERVER_SOCKET_BIND_FAIL;
		}

		NET_ERROR_CODE netError = SetNonBlockSocket(m_ServerSockFD);
		if (netError != NET_ERROR_CODE::kNONE) {
			return netError;
		}

		// 서버 소켓을 클라이언트 연결 요청을 받을 수 있는 상태로 만드는 함수 호출의 에러 확인
		if (listen(m_ServerSockFD, backlogCount) == SOCKET_ERROR) {
			return NET_ERROR_CODE::kSERVER_SOCKET_LISTEN_FAIL;
		}
		
		m_MaxSockFD = m_ServerSockFD;
		// %I64u ==  64비트 정수 출력
		m_pRefLogger->WriteLog(LOG_LEVEL::kL_INFO, "%s | 서버 리스닝 소켓(%I64u)", __FUNCTION__, m_ServerSockFD);
		return NET_ERROR_CODE::kNONE;
	}

	NET_ERROR_CODE TcpNetwork::SetNonBlockSocket(const SOCKET sock)
	{
		unsigned long mode = 1;
		// 소켓을 논블로킹(non-blocking) 모드로 설정하는 함수 호출의 에러 검사 구문
#ifdef _WIN32
		if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
#else
		if (ioctl(sock, FIONBIO, &mode) == -1) {
#endif
			return NET_ERROR_CODE::kSERVER_SOCKET_FIONBIO_FAIL;
		}
		
		return NET_ERROR_CODE::kNONE;
	}

	NET_ERROR_CODE TcpNetwork::SendData(const int32_t sessionIndex, const int16_t packetId, const int16_t bodySize, const char* pMsg)
	{
		ClientSession& session = m_ClientSessionPool[sessionIndex];

		// 버퍼 크기 확인
		int32_t pos = session.SendSize;
		int16_t totalSize = (int16_t)(bodySize + PACKET_HEADER_SIZE);
		if ((pos + totalSize) > m_Config.MaxClientSendBufferSize) {
			return NET_ERROR_CODE::kCLIENT_SEND_BUFFER_FULL;
		}

		PacketHeader pktHeader{ totalSize, packetId, (uint8_t)0 };
		// pSendBuffer[pos] 로 해더정보 복사
		memcpy(&session.pSendBuffer[pos], (uint8_t*)&pktHeader, PACKET_HEADER_SIZE);

		// body 가 있을 경우 pSendBuffer로 값 복사
		if (bodySize > 0) {
			memcpy(&session.pSendBuffer[pos + PACKET_HEADER_SIZE], pMsg, bodySize);
		}

		// 버퍼가 합쳐진 길이를 적용
		session.SendSize += totalSize;

		return NET_ERROR_CODE::kNONE;
	}


	void TcpNetwork::Run()
	{
		// 원본 m_Readfds를 직접 넘기면 감시할 소켓 집합이 select 호출에 의해 변경되어버림
		fd_set read_set = m_Readfds;
		fd_set write_set = m_Readfds;

		// 대기시간 1000 micro second => 1 milli second
		timeval timeout{ 0, 1000 };
		// 다수의 소켓 파일 디스크립터 상태를 검사(select)
#ifdef _WIN32
		int32_t selectResult = select(0, &read_set, &write_set, 0, &timeout);
#else
		int32_t selectResult = select(m_MaxSockFD + 1, &read_set, &write_set, 0, &timeout);
#endif
		bool isFDSetChanged = RunCheckSelectResult(selectResult);
		if (isFDSetChanged == false) {
			return;
		}

		// 검사 성공 후 fd_set 집합 안에 특정 fd가 있는지 확인
		if (FD_ISSET(m_ServerSockFD, &read_set)) {
			NewSession();
		}

		RunCheckSelectClients(read_set, write_set);
	}

	void TcpNetwork::Release()
	{
#ifdef _WIN32
		// 윈속 리소스를 해제하고 사용을 종료
		WSACleanup();
#endif
	}


	void TcpNetwork::ForcingClose(const int32_t sessionIndex)
	{
		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

		CloseSession(SOCKET_CLOSE_CASE::kFORCING_CLOSE, m_ClientSessionPool[sessionIndex].SocketFD, sessionIndex);
	}

	RecvPacketInfo TcpNetwork::GetPacketInfo()
	{
		RecvPacketInfo packetInfo;
		if (m_PacketQueue.empty() == false) {
			packetInfo = m_PacketQueue.front();
			m_PacketQueue.pop_front();
		}

		return packetInfo;
	}

	bool TcpNetwork::RunCheckSelectResult(const int32_t result)
	{
		// 타임아웃(0) 및 실패(-1)
		if (result == 0 || result == -1) {
			return false; 
		}
		
		return true;
	}

	void TcpNetwork::RunCheckSelectClients(fd_set& read_set, fd_set& wrtie_set)
	{
		// ++i -> 임시 객체를 만들지 않아 약간 더 효율적
		for (int32_t i = 0; i < m_ClientSessionPool.size(); ++i) {
			ClientSession& session = m_ClientSessionPool[i];
			if (session.IsConnected() == false) {
				continue;
			}

			SOCKET fd = session.SocketFD;
			int32_t sessionIndex = session.Index;

			// 읽기 검사
			bool reciveResult = RunProcessReceive(sessionIndex, fd, read_set);
			if (reciveResult == false) {
				continue; 
			}
			// 쓰기 검사
			RunProcessWrite(sessionIndex, fd, wrtie_set);
		}
	}

	bool TcpNetwork::RunProcessReceive(const int32_t sessionIndex, const SOCKET fd, fd_set& read_set)
	{
		if (!FD_ISSET(fd, &read_set)) {
			return true;
		}

		NET_ERROR_CODE result = RecvSocket(sessionIndex);
		if (result != NET_ERROR_CODE::kNONE) {
			CloseSession(SOCKET_CLOSE_CASE::kSOCKET_RECV_BUFFER_PROCESS_ERROR, fd, sessionIndex);
			return false;
		}

		result = RecvBufferProcess(sessionIndex);
		if (result != NET_ERROR_CODE::kNONE) {
			CloseSession(SOCKET_CLOSE_CASE::kSOCKET_RECV_BUFFER_PROCESS_ERROR, fd, sessionIndex);
			return false;
		}

		return true;
	}

	void TcpNetwork::RunProcessWrite(const int32_t sessionIndex, const SOCKET fd, fd_set& write_set)
	{
		if (!FD_ISSET(fd, &write_set)) {
			return;
		}

		NetError result = FlushSendBuff(sessionIndex);
		if (result.Error != NET_ERROR_CODE::kNONE) {
			CloseSession(SOCKET_CLOSE_CASE::kSOCKET_SEND_ERROR, fd, sessionIndex);
		}
	}

	// TCP 소켓에서 데이터를 비동기(논블로킹) 방식으로 수신
	// 내부 버퍼 상태를 관리하는 역할을 하는 코드
	NET_ERROR_CODE TcpNetwork::RecvSocket(const int32_t sessionIndex)
	{
		ClientSession& session = m_ClientSessionPool[sessionIndex];
		if (session.IsConnected() == false) {
			return NET_ERROR_CODE::kRECV_PROCESS_NOT_CONNECTED;
		}

		
		int32_t recvPos = 0;
		if (session.RemainingDataSize > 0) {
			// 이전에 읽은 위치 부터 남은 데이터를pRecvBuffer 시작 위치로 복사
			memcpy(session.pRecvBuffer, 
				&session.pRecvBuffer[session.PrevReadPosInRecvBuffer], 
				session.RemainingDataSize);
			// recvPos 를 이동시켜 받을 데이터가 뒤에 들어가도록 함
			recvPos += session.RemainingDataSize;
		}

		//  명시적 형변환(casting)
		SOCKET fd = static_cast<SOCKET>(session.SocketFD);
		// 소켓 recv 호출로 데이터 수신 -> 비어 있는 버퍼 공간에 최대 (MAX_PACKET_BODY_SIZE * 2) 바이트만큼 읽음
		// 버퍼 내에 불완전하게 도착한 이전 패킷 데이터 + 다음 패킷 데이터가 같이 들어올 때를 대비해 2배로 지정
		int32_t recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);
		if (recvSize == 0) {
			return NET_ERROR_CODE::kRECV_REMOTE_CLOSE;
		}

		// recvSize가 음수 일 떄 == 에러 발생 시
		if (recvSize < 0) {
#ifdef _WIN32
			// 에러가 "잠시 기다려야 함" 이면 에러 NONE 반환
			int32_t netError = WSAGetLastError();
			if (netError != WSAEWOULDBLOCK) {
#else
			int32_t netError = errno;
			if (netError != EAGAIN && netError != EWOULDBLOCK) {
#endif
			// 다른 오류인 경우 API 오류로 간주
			return NET_ERROR_CODE::kRECV_API_ERROR;
			}
			else {
				return NET_ERROR_CODE::kNONE;
			}
		}

		session.RemainingDataSize += recvSize;
		return NET_ERROR_CODE::kNONE;
	}

	NET_ERROR_CODE TcpNetwork::RecvBufferProcess(const int32_t sessionIndex)
	{
		ClientSession& session = m_ClientSessionPool[sessionIndex];
		const int32_t dataSize = session.RemainingDataSize;
		int32_t readPos = 0;
		PacketHeader* pPktHeader;

		while ((dataSize - readPos) >= PACKET_HEADER_SIZE) {
			pPktHeader = (PacketHeader*)&session.pRecvBuffer[readPos];
			readPos += PACKET_HEADER_SIZE;

			int16_t bodySize = (int16_t)(pPktHeader->TotalSize - PACKET_HEADER_SIZE);
			if (bodySize > 0) {
				if (bodySize > (dataSize - readPos)) {
					readPos -= PACKET_HEADER_SIZE;
					break;
				}

				if (bodySize > MAX_PACKET_BODY_SIZE) {
					return NET_ERROR_CODE::kRECV_CLIENT_MAX_PACKET;
				}
			}
			
			AddPacketQueue(sessionIndex, pPktHeader->Id, bodySize, &session.pRecvBuffer[readPos]);
			readPos += bodySize;
		}

		session.RemainingDataSize -= readPos;
		session.PrevReadPosInRecvBuffer = readPos;

		return NET_ERROR_CODE::kNONE;
	}

	// 버퍼에 있는 내용을 즉시 출력 장치(파일, 화면, 네트워크 등)로 밀어내서 반영
	NetError TcpNetwork::FlushSendBuff(const int32_t sessionIndex)
	{
		ClientSession& session = m_ClientSessionPool[sessionIndex];
		if (session.IsConnected() == false) {
			return NetError(NET_ERROR_CODE::kCLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE);
		}

		SOCKET fd = static_cast<SOCKET>(session.SocketFD);
		NetError result = SendSocket(fd, session.pSendBuffer, session.SendSize);
		if (result.Error != NET_ERROR_CODE::kNONE) {
			return result;
		}

		int32_t sendSize = result.Value;
		if (sendSize < session.SendSize) {
			// 보내지 않은 남은 데이터를 앞으로 당겨서 정리
			memmove(&session.pSendBuffer[0],
				&session.pSendBuffer[sendSize],
				session.SendSize - sendSize);

			session.SendSize -= sendSize;
		}
		else {
			session.SendSize = 0;
		}

		return result;
	}


	NetError TcpNetwork::SendSocket(const SOCKET fd, const char* pMsg, const int32_t size)
	{
		NetError result(NET_ERROR_CODE::kNONE);

		if (size <= 0) {
			return result;
		}

		// TCP 소켓을 통해 데이터를 전송
		result.Value = send(fd, pMsg, size, 0);
		if (result.Value <= 0) {
			result.Error = NET_ERROR_CODE::kSEND_SIZE_ZERO;
		}

		return result;
	}

	void TcpNetwork::CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int32_t sessionIndex)
	{
		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

#ifdef _WIN32
		closesocket(sockFD);
#else
		close(sockFD);
#endif
		FD_CLR(sockFD, &m_Readfds);

		if (closeCase == SOCKET_CLOSE_CASE::kSESSION_POOL_EMPTY) {
			return;
		}

		ReleaseSessionIndex(sessionIndex);
		--m_ConnectedSessionCount;

		AddPacketQueue(sessionIndex, (int16_t)PACKET_ID::kNTF_SYS_CLOSE_SESSION, 0, nullptr);
	}

	void TcpNetwork::ReleaseSessionIndex(const int32_t index)
	{
		// '재사용 가능한 인덱스 목록'에 추가
		m_ClientSessionPoolIndex.push_back(index);
		// 세션 초기화
		m_ClientSessionPool[index].Clear();
	}

	void TcpNetwork::AddPacketQueue(const int32_t sessionIndex, const int16_t pktId, const int16_t bodySize, int8_t* pDataPos)
	{
		RecvPacketInfo packetInfo;
		packetInfo.SessionIndex = sessionIndex;
		packetInfo.PacketId = pktId;
		packetInfo.PacketBodySize = bodySize;
		packetInfo.pRefData = pDataPos;

		m_PacketQueue.push_back(packetInfo);
	}

	NET_ERROR_CODE TcpNetwork::NewSession()
	{
		int32_t tryCount = 0;

		do {
			++tryCount;

			struct sockaddr_in client_adr;
#ifdef _WIN32
			int32_t client_len = static_cast<int32_t>(sizeof(client_adr));
			SOCKET client_sockFD = accept(m_ServerSockFD, (struct sockaddr*)&client_adr, &client_len);
#else
			int32_t client_len = sizeof(client_adr);
			SOCKET client_sockFD = accpet(m_ServerSockFD, (struct sockaddr*)&client_adr, (socklen_t*)&client_len);
#endif
			if (client_sockFD == INVALID_SOCKET) {
#ifdef _WIN32
				int32_t netError = WSAGetLastError();
				if (netError == WSAEWOULDBLOCK) {
#else 
				int32_t netError = errno;
				if (netError == EAGAIN && netError == EWOULDBLOCK) {
#endif
					return NET_ERROR_CODE::kACCEPT_API_WSAEWOULDBLOCK;
				}

				m_pRefLogger->WriteLog(LOG_LEVEL::kL_ERROR, "%s | 잘못된 소켓 등록", __FUNCTION__);
				return NET_ERROR_CODE::kACCEPT_API_ERROR;
			}

			int32_t newSessionIndex = AllocClientSessionIndex();
			if (newSessionIndex < 0) {
				m_pRefLogger->WriteLog(LOG_LEVEL::kL_WARN, "%s | 클라이언트 소켓(%I64u) 세션 최대치", __FUNCTION__, client_sockFD);
			
				CloseSession(SOCKET_CLOSE_CASE::kSESSION_POOL_EMPTY, client_sockFD, -1);
				return NET_ERROR_CODE::kACCEPT_MAX_SESSION_COUNT;
			}

			// IPv4 주소 문자열화
			char clientIP[MAX_IP_LEN] = { 0 };
			// .(멤버 접근 연산자)는 우선순위가 &보다 높기에 괄호로 묶음
			inet_ntop(AF_INET, &(client_adr.sin_addr), clientIP, MAX_IP_LEN - 1);

			SetSockOption(client_sockFD);
			SetNonBlockSocket(client_sockFD);

			FD_SET(client_sockFD, &m_Readfds);
			ConnectedSession(newSessionIndex, client_sockFD, clientIP);
		} while (tryCount < FD_SETSIZE);

		return NET_ERROR_CODE::kNONE;
	}

	int32_t TcpNetwork::AllocClientSessionIndex()
	{
		if (m_ClientSessionPoolIndex.empty()) {
			return -1;
		}

		//  맨 앞(front)에 있는 값을 꺼내 index에 저장한 뒤, 그 값을 컨테이너에서 삭제
		int32_t index = m_ClientSessionPoolIndex.front();
		m_ClientSessionPoolIndex.pop_front();
		return index;
	}

	void TcpNetwork::SetSockOption(const SOCKET fd)
	{
		// 소켓이 닫힐 때 남아있는 데이터를 어떻게 처리할지 결정하는 설정
		linger ling;
		// 기본 동작, 소켓을 닫을 때 별도의 대기 없이 자연스럽게 종료
		ling.l_onoff = 0;
		// linger 대기 시간 0초 (사용 안 함)
		ling.l_linger = 0;
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

		// 소켓의 수신,송신 버퍼 크기를 size (설정값) 바이트로 지정
		int size1 = m_Config.MaxClientSocketOptRecvBufferSize;
		int size2 = m_Config.MaxClientSocketOptSendBufferSize;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size1, sizeof(size1));
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&size2, sizeof(size2));
	}

	void TcpNetwork::ConnectedSession(const int32_t sessionIndex, const SOCKET fd, const char* pIP)
	{
		if (m_MaxSockFD < fd) {
			m_MaxSockFD = fd;
		}

		++m_ConnectSeq;

		// m_ClientSessionPool 에 있는 세션에 정보 업데이트
		ClientSession& session = m_ClientSessionPool[sessionIndex];
		session.Seq = m_ConnectSeq;
		session.SocketFD = fd;
		memcpy(session.IP, pIP, MAX_IP_LEN - 1);

		++m_ConnectedSessionCount;

		AddPacketQueue(sessionIndex, (int16_t)PACKET_ID::kNTF_SYS_CONNECT_SESSION, 0, nullptr);
		m_pRefLogger->WriteLog(LOG_LEVEL::kL_INFO, "%s | 새로운 세션 소켓(%I64u), m_ConnectSeq(%d), IP(%s)", __FUNCTION__, fd, m_ConnectSeq, pIP);
	}
	
}
