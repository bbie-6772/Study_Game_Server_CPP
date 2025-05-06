#pragma once

#include <mutex>

#ifdef _WIN32
// 경고 설정 저장
#pragma warning( push )
// 형 변환 데이터 손실 가능성 경고 무시
#pragma warning( disable : 4244 )
// conmainp 헤더 파일을 사용(+using을 통해 사용하기 쉽게 설정)
#include "../Common/conmanip.h"
using namespace conmanip;
// 앞서 저장한 경고 설정(push)을 복원
#pragma warning( pop )
#endif

#include "../ServerNetLib/ILog.h"

namespace NLogicLib
{
	// 기존 public 멤버를 똑같이 public 취급으로 상속받기
	class ConsoleLog : public NServerNetLib::ILog
	{
	public:
		virtual ~ConsoleLog() = default;
#ifdef _WIN32
		ConsoleLog()
		{
			console_out_context ctxout;
			m_Conout = ctxout;
		}

		//  뮤텍스 잠금은 '출력 동기화와 무결성 보장'이 목적
	protected:
		virtual void Error(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << settextcolor(console_text_colors::light_red) << "[오류] | " << pText << std::endl;
		}

		virtual void Warn(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << settextcolor(console_text_colors::light_yellow) << "[경고] | " << pText << std::endl;
		}

		virtual void Debug(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << settextcolor(console_text_colors::light_white) << "[디버깅] | " << pText << std::endl;
		}

		virtual void Trace(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << settextcolor(console_text_colors::light_green) << "[추적] | " << pText << std::endl;
		}

		virtual void Error(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << settextcolor(console_text_colors::green) << "[정보] | " << pText << std::endl;
		}


	private:
		console_out m_Conout;
		std::mutex m_lock;
	};
#else
		ConsoleLog() = default;
	protected:
		virtual void Error(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << "[오류] | " << pText << std::endl;
		}

		virtual void Warn(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << "[경고] | " << pText << std::endl;
		}

		virtual void Debug(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << "[디버깅] | " << pText << std::endl;
		}

		virtual void Trace(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << "[추적] | " << pText << std::endl;
		}

		virtual void Info(const char* pText) override
		{
			std::lock_guard<std::mutex> guard(m_lock);
			std::cout << "[정보] | " << pText << std::endl;
		}

	private:
		std::mutex m_lock;

	};
#endif
}