#pragma once

#include "define.h"
#include "server_network_error_code.h"
#include "interface_log.h"

namespace NServerNetLib
{
	class ITcpNetwork
	{
	public:
		ITcpNetwork() {}

		virtual ~ITcpNetwork() {}

		virtual NET_ERROR_CODE Init(const ServerConfig* pConfig, ILog* pLogger) { return NET_ERROR_CODE::kNONE;  }

		virtual NET_ERROR_CODE SendData(const int32_t sessionIndex, const int16_t packetId,
			const int16_t bodySize, const char* pMsg) {
			return NET_ERROR_CODE::kNONE;
		}

		virtual void Run() {}

		virtual void Release() {}

		virtual void ForcingClose(const int32_t sessionIndex) {}

		virtual int32_t ClientSessionPoolSize() { return 0;  }

		virtual RecvPacketInfo GetPacketInfo() { return RecvPacketInfo(); }

	};
}