#pragma once

#include <cstdint>

namespace NServerNetLib
{
	enum class NET_ERROR_CODE : int16_t
    {
        // 기본 값: 에러 없음
        kNONE = 0,

        // 서버 소켓 생성 및 설정 관련 에러
        kSERVER_SOCKET_CREATE_FAIL = 11,
        kSERVER_SOCKET_SO_REUSEADDR_FAIL = 12,
        kSERVER_SOCKET_BIND_FAIL = 14,
        kSERVER_SOCKET_LISTEN_FAIL = 15,
        kSERVER_SOCKET_FIONBIO_FAIL = 16,

        // 송신 관련 에러 및 상태
        kSEND_CLOSE_SOCKET = 21,
        kSEND_SIZE_ZERO = 22,
        kCLIENT_SEND_BUFFER_FULL = 23,
        kCLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE = 24,

        // 수락(accept) 처리 관련 에러
        kACCEPT_API_ERROR = 26,
        kACCEPT_MAX_SESSION_COUNT = 27,
        kACCEPT_API_WSAEWOULDBLOCK = 28,

        // 수신 관련 에러
        kRECV_API_ERROR = 32,
        kRECV_BUFFER_OVERFLOW = 33,
        kRECV_REMOTE_CLOSE = 34,
        kRECV_PROCESS_NOT_CONNECTED = 35,
        kRECV_CLIENT_MAX_PACKET = 36,
    };

    constexpr int MAX_NET_ERROR_STRING_LENGTH = 64;

    struct NetError
    {
        NetError(NET_ERROR_CODE code)
        {
            Error = code;
        }

        NET_ERROR_CODE Error = NET_ERROR_CODE::kNONE;
        // 다국어 텍스트, 유니코드 문자 처리 필요할 때 사용
        wchar_t Msg[MAX_NET_ERROR_STRING_LENGTH] = { 0 };
        int32_t Value = 0;
    };

}