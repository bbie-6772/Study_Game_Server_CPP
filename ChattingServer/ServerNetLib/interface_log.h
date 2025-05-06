#pragma once

#include <stdio.h>
#include <stdarg.h>

// 특정 경고 메시지를 무시
// 함수의 매개변수 중 사용하지 않는 매개변수에 대해서 경고 4100을 발생 X
#pragma warning(disable : 4100)

// 네트워크 서버 라이브러리
namespace NServerNetLib
{
	// 버퍼 크기를 미리 정해두어 필요한 만큼만 메모리를 할당
	constexpr int MAX_LOG_STRING_LENGTH = 1024;

	enum class LOG_LEVEL : int16_t
	{
		// 로그의 가장 상세한 추적용 메시지(trace messages) 
		kL_TRACE = 1,
		kL_DEBUG = 2,
		kL_WARN = 3,
		kL_ERROR = 4,
		kL_INFO = 5,
	};

	class ILog
	{
	public:
		ILog() {}
		virtual ~ILog() {}

		// pFormat에는 "Value: %d, Name: %s"처럼 문자열 포맷
		virtual void WriteLog(const LOG_LEVEL level, const char* pFormat, ...)
		{
			char szText[MAX_LOG_STRING_LENGTH];

			// args에는 그 포맷에 들어갈 실제 값들(예: 정수, 문자열)이 가변 인자로 처리
			va_list args;
			va_start(args, pFormat);
// 운영체제 별 로그 문자열 포맷(오버플로우 방지) 하기
#ifdef _WIN32
			vsnprintf_s(szText, MAX_LOG_STRING_LENGTH, pFormat, args);
#else
			vsnprintf(szText, MAX_LOG_STRING_LENGTH, pFormat, args);
#endif
			va_end(args);
			
			switch (level)
			{
			case LOG_LEVEL::kL_TRACE:
				Trace(szText);
				break;
			case LOG_LEVEL::kL_DEBUG:
				Debug(szText);
				break;
			case LOG_LEVEL::kL_WARN:
				Warn(szText);
				break;
			case LOG_LEVEL::kL_ERROR:
				Error(szText);
				break;
			case LOG_LEVEL::kL_INFO:
				Info(szText);
				break;
			default:
				break;
			}
		};

	protected:
		virtual void Trace(const char* format) = 0;
		virtual void Debug(const char* format) = 0;
		virtual void Warn(const char* format) = 0;
		virtual void Error(const char* format) = 0;
		virtual void Info(const char* format) = 0;
	};

}
