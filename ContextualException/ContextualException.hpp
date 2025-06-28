#ifndef __CONTEXTUAL_EXCEPTION_HPP__
#define __CONTEXTUAL_EXCEPTION_HPP__

#include <cstring>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

// __FILENAME__ : 소스 파일명 출력
#ifndef __FILENAME__
#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || \
    defined(__MINGW32__) || defined(__BORLANDC__)
#define __FILENAME__ \
    (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#endif

// MSVC C++11 탐지가 불완전하므로 리터럴 지원 여부로 판별
#if defined(__cpp_user_defined_literals)
#define CONTEXTUAL_EXCEPTION_NOEXCEPT noexcept
#else
#define CONTEXTUAL_EXCEPTION_NOEXCEPT
#endif

class ContextualException : public std::exception {
   public:
    // 추적 프레임
    struct Frame {
        std::string message;
        int code;

        std::string file;
        int line;
        std::string function;

        int depth;

        Frame() : code(0), line(0), depth(0) {}
        Frame(const std::string& message, int code, const std::string& file,
              int line, const std::string& function)
            : message(message),
              code(code),
              file(file),
              line(line),
              function(function),
              depth(0) {}
    };

   public:
    ContextualException() {}
    ContextualException(const std::string& message, const std::string& file,
                        int line, const std::string& function) {
        const int default_code = 0;
        SetBaseFrame(message, default_code, file, line, function);
        AssignErrorMessage();
    }
    ContextualException(const std::string& message, int code,
                        const std::string& file, int line,
                        const std::string& function) {
        SetBaseFrame(message, code, file, line, function);
        AssignErrorMessage();
    }
    ContextualException(const std::string& message,
                        const std::exception& exception,
                        const std::string& file, int line,
                        const std::string& function) {
        const int default_code = 0;
        SetBaseFrame(message, default_code, file, line, function);
        WrapException(exception);
        AssignErrorMessage();
    }
    ContextualException(const std::string& message, int code,
                        const std::exception& exception,
                        const std::string& file, int line,
                        const std::string& function) {
        SetBaseFrame(message, code, file, line, function);
        WrapException(exception);
        AssignErrorMessage();
    }

    virtual ~ContextualException() CONTEXTUAL_EXCEPTION_NOEXCEPT {};

   public:
    virtual const char* what() const CONTEXTUAL_EXCEPTION_NOEXCEPT override {
        return error_message_.c_str();
    }

   public:
    const std::string& Message() const {
        return base_frame_.message;
    }
    int Code() const {
        return base_frame_.code;
    }

    const std::string& File() const {
        return base_frame_.file;
    }
    int Line() const {
        return base_frame_.line;
    }
    const std::string& Function() const {
        return base_frame_.function;
    }

    std::string DetailedErrorMessage() const {
        std::ostringstream stream;
        stream << error_message_;

        auto size_frames = child_frames_.size();
        for (size_t ii = 0; ii < size_frames; ++ii) {
            stream << "\n    " << GetFrameMessage(child_frames_[ii]);
        }

        return stream.str();
    }

   public:
    void AppendException(const ContextualException& exception) {
        AppendFramesFrom(exception);
        NormalizeChildDepth();
    }

   private:
    void SetBaseFrame(const std::string& message, int code,
                      const std::string& file, int line,
                      const std::string& function) {
        base_frame_ = Frame(message, code, file, line, function);
    }

    void AssignErrorMessage() {
        error_message_ = GetFrameMessage(base_frame_);
    }

    void WrapException(const std::exception& exception) {
        if (IsContextualException(exception)) {
            AppendFramesFrom(exception);
        } else {
            WrapOtherException(exception);
        }
        NormalizeChildDepth();
    }
    void AppendFramesFrom(const std::exception& exception) {
        const auto* origin_exception =
            dynamic_cast<const ContextualException*>(&exception);
        child_frames_.emplace_back(origin_exception->base_frame_);
        child_frames_.insert(child_frames_.end(),
                             origin_exception->child_frames_.begin(),
                             origin_exception->child_frames_.end());
    }
    void WrapOtherException(const std::exception& exception) {
        auto& frame = base_frame_;
        if (frame.message.empty()) {
            frame.message = exception.what();
        } else {
            frame.message += ", ";
            frame.message += exception.what();
        }
    }

    static bool IsContextualException(const std::exception& exception) {
        return dynamic_cast<const ContextualException*>(&exception) != nullptr;
    }

    std::string GetFrameMessage(const Frame& frame) const {
        std::ostringstream stream;
        stream << frame.file << ":" << frame.line << " | " << frame.function
               << "() | ";
        if (0 != frame.code) {
            stream << "[code=" << frame.code << "] ";
        }
        stream << frame.message;
        return stream.str();
    }

    void NormalizeChildDepth() {
        auto size_frames = child_frames_.size();
        for (size_t ii = 0; ii < size_frames; ++ii) {
            child_frames_[ii].depth = static_cast<int>(ii) + 1;
        }
    }

   private:
    Frame base_frame_;
    std::vector<Frame> child_frames_;
    std::string error_message_;
};

namespace contextual_exception {
namespace anonymous {

inline ContextualException Make(const char* file, int line,
                                const char* function,
                                const std::string& message) {
    return ContextualException(message, file, line, function);
}

inline ContextualException Make(const char* file, int line,
                                const char* function,
                                const std::string& message, int code) {
    return ContextualException(message, code, file, line, function);
}

inline ContextualException Wrap(const char* file, int line,
                                const char* function,
                                const std::string& message,
                                const std::exception& exception) {
    return ContextualException(message, exception, file, line, function);
}

inline ContextualException Wrap(const char* file, int line,
                                const char* function,
                                const std::string& message, int code,
                                const std::exception& exception) {
    return ContextualException(message, code, exception, file, line, function);
}

inline ContextualException* SafeChain(
    const char* file, int line, const char* function,
    const std::string& message, ContextualException* source_exception_or_null) {
    if (!source_exception_or_null) {
        return nullptr;
    }

    ContextualException lower_exception = std::move(*source_exception_or_null);
    *source_exception_or_null =
        ContextualException(message, file, line, function);
    source_exception_or_null->AppendException(lower_exception);

    return source_exception_or_null;
}

inline ContextualException* SafeChain(
    const char* file, int line, const char* function,
    const std::string& message, int code,
    ContextualException* source_exception_or_null) {
    if (!source_exception_or_null) {
        return nullptr;
    }

    ContextualException lower_exception = std::move(*source_exception_or_null);
    *source_exception_or_null =
        ContextualException(message, code, file, line, function);
    source_exception_or_null->AppendException(lower_exception);

    return source_exception_or_null;
}

}  // namespace anonymous
}  // namespace contextual_exception

// 인자 개수 계산 매크로
#define __CONTEXTUAL_GET_MACRO_ARGUMENTS_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define __CONTEXTUAL_EXPAND_MACRO(x) x

// usecase 1) CONTEXTUAL_EXCEPTION(message, code)
// usecase 2) CONTEXTUAL_EXCEPTION(message)
#define CONTEXTUAL_EXCEPTION(...)                                   \
    ::contextual_exception::anonymous::Make(__FILENAME__, __LINE__, \
                                            __FUNCTION__, __VA_ARGS__)

// usecase 1) WRAP_CONTEXTUAL_EXCEPTION(message, code, std::exception)
// usecase 2) WRAP_CONTEXTUAL_EXCEPTION(message, std::exception)
#define WRAP_CONTEXTUAL_EXCEPTION(...)                              \
    ::contextual_exception::anonymous::Wrap(__FILENAME__, __LINE__, \
                                            __FUNCTION__, __VA_ARGS__)

// usecase 1) SAFE_CHAIN_CONTEXTUAL_EXCEPTION(message, code, std::exception *)
// usecase 2) SAFE_CHAIN_CONTEXTUAL_EXCEPTION(message, std::exception *)
#define SAFE_CHAIN_CONTEXTUAL_EXCEPTION(...)                             \
    ::contextual_exception::anonymous::SafeChain(__FILENAME__, __LINE__, \
                                                 __FUNCTION__, __VA_ARGS__)

// 내부 구현: 2개 인자 버전 (message, exception_ptr)
#define __CHAIN_CONTEXTUAL_EXCEPTION_WITHOUT_CODE(message, exception_ptr) \
    (exception_ptr) = SAFE_CHAIN_CONTEXTUAL_EXCEPTION(message, exception_ptr)
// 내부 구현: 3개 인자 버전 (message, code, exception_ptr)
#define __CHAIN_CONTEXTUAL_EXCEPTION_WITH_CODE(message, code, exception_ptr) \
    (exception_ptr) =                                                        \
        SAFE_CHAIN_CONTEXTUAL_EXCEPTION(message, code, exception_ptr)

// 적절한 구현 선택 매크로
#define __CHOOSE_CHAIN_CONTEXTUAL_EXCEPTION_MACRO(...)                \
    __CONTEXTUAL_EXPAND_MACRO(__CONTEXTUAL_GET_MACRO_ARGUMENTS_COUNT( \
        __VA_ARGS__, __CHAIN_CONTEXTUAL_EXCEPTION_WITH_CODE,          \
        __CHAIN_CONTEXTUAL_EXCEPTION_WITHOUT_CODE))

// usecase 1) SAFE_CHAIN_CONTEXTUAL_EXCEPTION_TO(message, code, std::exception *)
// usecase 2) SAFE_CHAIN_CONTEXTUAL_EXCEPTION_TO(message, std::exception *)
#define SAFE_CHAIN_CONTEXTUAL_EXCEPTION_TO(...) \
    __CONTEXTUAL_EXPAND_MACRO(                  \
        __CHOOSE_CHAIN_CONTEXTUAL_EXCEPTION_MACRO(__VA_ARGS__)(__VA_ARGS__))

// usecase 1) THROW_CONTEXTUAL_EXCEPTION(message, code)
// usecase 2) THROW_CONTEXTUAL_EXCEPTION(message)
#define THROW_CONTEXTUAL_EXCEPTION(...) throw CONTEXTUAL_EXCEPTION(__VA_ARGS__)

// usecase 1) THROW_WRAP_CONTEXTUAL_EXCEPTION(message, code, std::exception)
// usecase 2) THROW_WRAP_CONTEXTUAL_EXCEPTION(message, std::exception)
#define THROW_WRAP_CONTEXTUAL_EXCEPTION(...) \
    throw WRAP_CONTEXTUAL_EXCEPTION(__VA_ARGS__)

#endif  //__CONTEXTUAL_EXCEPTION_HPP__