#include "Core/Error.h"

#include "Platform/PlatformUtils.h"

namespace Life
{
    namespace ErrorHandling
    {
        static ErrorHandler s_ErrorHandler = DefaultErrorHandler;

        void SetErrorHandler(ErrorHandler handler)
        {
            s_ErrorHandler = handler ? handler : DefaultErrorHandler;
        }

        ErrorHandler GetErrorHandler()
        {
            return s_ErrorHandler;
        }

        void DefaultErrorHandler(const Error& error)
        {
            // Log the error
            Error::LogError(error);

            // For fatal errors, break into debugger if available
            if (error.IsFatal())
            {
                PlatformUtils::BreakIntoDebugger();
            }
        }

        void Assert(bool condition, const std::string& message, const std::source_location& location)
        {
            if (!condition)
            {
                Error error(ErrorCode::Unknown, "Assertion failed: " + message, location, ErrorSeverity::Critical);
                s_ErrorHandler(error);
                throw error;
            }
        }

        void Verify(bool condition, const std::string& message, const std::source_location& location)
        {
            if (!condition)
            {
                Error error(ErrorCode::InvalidState, "Verification failed: " + message, location, ErrorSeverity::Error);
                s_ErrorHandler(error);
                throw error;
            }
        }

        Result<void> TryVoid(const std::function<void()>& func)
        {
            try
            {
                func();
                return Result<void>();
            }
            catch (const Error& error)
            {
                return Result<void>(error);
            }
            catch (const std::exception& e)
            {
                return Result<void>(Error(ErrorCode::Unknown, e.what(), std::source_location::current()));
            }
            catch (...)
            {
                return Result<void>(Error(ErrorCode::Unknown, "Unknown exception", std::source_location::current()));
            }
        }
    }
}
