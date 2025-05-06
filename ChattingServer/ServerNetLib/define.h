#pragma once

#include <cstdint>

namespace NServerNetLib
{
	struct ServerConfig
	{
		// 16비트 부호 없는 정수형이라 값 범위가 0부터 65535까지 << 포트와 동일
		uint16_t Port;
		uint32_t BackLogCount;

		// 로그인에서 관리가능한 MaxClientCount + 여유분
		uint32_t MaxClientCount;
		uint32_t ExtraClientCount;

		// SocketOpt는 "Socket Option"(소켓 옵션)을 의미
		uint16_t MaxClientSocketOptRecvBufferSize;
		uint16_t MaxClientSocketOptSendBufferSize;
		uint16_t MaxClientRecvBufferSize;
		uint16_t MaxClientSendBufferSize;

		// 연결 후 특정 시간 내 로그인 완료 여부 확인
		bool IsLoginCheck;

		uint32_t MaxLobbyCount;
		uint32_t MaxLobbyUserCount;
		uint32_t MaxRoomCountByLobby;
		uint32_t MaxRoomUserCount;
	};

	// IP 문자열 최대 길이 
	constexpr int MAX_IP_LEN = 32;
	// 최대 패킷 크기
	constexpr int MAX_PACKET_BODY_SIZE = 1024;

	struct ClientSession
	{
		bool IsConnected() { return SocketFD != 0 ? true : false; }

		void Clear()
		{
			Seq = 0;
			SocketFD = 0;
			// 존재하는 배열을 재활용하면서 빈 문자열로 초기화하는 방식
			IP[0] = '\0';
			RemainingDataSize = 0;
			PrevReadPosInRecvBuffer = 0;
			SendSize = 0;
		}

		int32_t Index = 0;
		int64_t Seq = 0;
		uint64_t SocketFD = 0;
		// 배열의 모든 원소를 0으로 초기화 ->  빈 문자열 상태 생성
		char IP[MAX_IP_LEN] = {0};

		char* pRecvBuffer = nullptr;
		int32_t RemainingDataSize = 0;
		int32_t PrevReadPosInRecvBuffer = 0;

		char* pSendBuffer = nullptr;
		int32_t SendSize = 0;
	};

	struct RecvPacketInfo
	{
		int32_t SessionIndex = 0;
		int16_t PacketId = 0;
		int16_t PacketBodySize = 0;
		uint8_t* pRefData = 0;
	};

	enum class SOCKET_CLOSE_CASE : int16_t
	{
		kSESSION_POOL_EMPTY = 1,
		kSELECT_ERROR = 2,
		kSOCKET_RECV_ERROR = 3,
		kSOCKET_RECV_BUFFER_PROCESS_ERROR = 4,
		kSOCKET_SEND_ERROR = 5,
		kFORCING_CLOSE = 6
	};

	enum class PACKET_ID : int16_t
	{
		kNTF_SYS_CONNECT_SESSION = 1,
		kNTF_SYS_CLOSE_SESSION = 2,
	};

// 구조체(또는 공용체 등)의 메모리 정렬(padding)을 1바이트 단위로 맞춤을 의미
// 컴파일러는 보통 구조체 크기를 멤버 중 가장 큰 타입의 정렬 단위 기준으로 맞춤
#pragma pack(push, 1)
	struct PacketHeader
	{
		int16_t TotalSize;
		int16_t Id;
		// 예약 공간을 미리 확보하는 용도
		uint8_t Reserve;
	};
	constexpr int PACKET_HEADER_SIZE = sizeof(PacketHeader);

	// 시스템의 세션(연결)을 닫는(종료하는) 알림 패킷
	struct PkNtfSysCloseSEssion : PacketHeader
	{
		int32_t SockFD;
	};
#pragma pack(pop)
}