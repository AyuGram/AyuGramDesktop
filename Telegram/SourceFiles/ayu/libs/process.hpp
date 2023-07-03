///////////////////////////////////////////////////////////
/// Copyright 2019 Alexy Pellegrini
///
/// Permission is hereby granted, free of charge,
/// to any person obtaining a copy of this software
/// and associated documentation files (the "Software"),
/// to deal in the Software without restriction,
/// including without limitation the rights to use,
/// copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so,
/// subject to the following conditions:
///
/// The above copyright notice and this permission notice
/// shall be included in all copies or substantial portions
/// of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
/// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
/// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
/// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
/// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
/// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
/// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
/// OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////

#ifndef NOT_ENOUGH_STANDARDS_PROCESS
#define NOT_ENOUGH_STANDARDS_PROCESS

#if defined(_WIN32)
    #define NES_WIN32_PROCESS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #define NES_POSIX_PROCESS
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <string.h>
    #include <limits.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#if __has_include("pipe.hpp")
    #define NES_PROCESS_PIPE_EXTENSION
    #include "pipe.hpp"
#endif

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <functional>
#include <cassert>
#include <utility>

#if defined(NES_WIN32_PROCESS)

namespace nes
{

class process;

namespace impl
{

enum class id_t : DWORD{};

constexpr bool operator==(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) == static_cast<DWORD>(rhs);
}

constexpr bool operator!=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) != static_cast<DWORD>(rhs);
}

constexpr bool operator<(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) < static_cast<DWORD>(rhs);
}

constexpr bool operator<=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) <= static_cast<DWORD>(rhs);
}

constexpr bool operator>(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) > static_cast<DWORD>(rhs);
}

constexpr bool operator>=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<DWORD>(lhs) >= static_cast<DWORD>(rhs);
}

struct auto_handle
{
public:
    constexpr auto_handle() = default;
    auto_handle(HANDLE h)
    :m_handle{h}
    {

    }

    ~auto_handle()
    {
        if(m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
    }

    auto_handle(const auto_handle&) = delete;
    auto_handle& operator=(const auto_handle&) = delete;

    auto_handle(auto_handle&& other) noexcept
    :m_handle{std::exchange(other.m_handle, INVALID_HANDLE_VALUE)}
    {

    }

    auto_handle& operator=(auto_handle&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, m_handle);

        return *this;
    }

    HANDLE release() noexcept
    {
        return std::exchange(m_handle, INVALID_HANDLE_VALUE);
    }

    operator HANDLE() const noexcept
    {
        return m_handle;
    }

    HANDLE* operator&() noexcept
    {
        return &m_handle;
    }

    const HANDLE* operator&() const noexcept
    {
        return &m_handle;
    }

    operator bool() const noexcept
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE m_handle{INVALID_HANDLE_VALUE};
};

}

enum class process_options : std::uint32_t
{
    none = 0x00,
#ifdef NES_PROCESS_PIPE_EXTENSION
    grab_stdout = 0x10,
    grab_stderr = 0x20,
    grab_stdin = 0x40
#endif
};

constexpr process_options operator&(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr process_options& operator&=(process_options& left, process_options right) noexcept
{
    left = left & right;
    return left;
}

constexpr process_options operator|(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr process_options& operator|=(process_options& left, process_options right) noexcept
{
    left = left | right;
    return left;
}

constexpr process_options operator^(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}

constexpr process_options& operator^=(process_options& left, process_options right) noexcept
{
    left = left ^ right;
    return left;
}

constexpr process_options operator~(process_options value) noexcept
{
    return static_cast<process_options>(~static_cast<std::uint32_t>(value));
}

class process
{
public:
    using native_handle_type = HANDLE;
    using return_code_type = DWORD;
    using id = impl::id_t;

public:
    constexpr process() noexcept = default;

    explicit process(const std::string& path, const std::string& working_directory)
    :process{path, {}, working_directory, {}}{}

    explicit process(const std::string& path, process_options options)
    :process{path, {}, {}, options}{}

    explicit process(const std::string& path, const std::vector<std::string>& args, process_options options)
    :process{path, args, {}, options}{}

    explicit process(const std::string& path, const std::string& working_directory, process_options options)
    :process{path, {}, working_directory, options}{}

    explicit process(const std::string& path, std::vector<std::string> args = std::vector<std::string>{}, const std::string& working_directory = std::string{}, process_options options [[maybe_unused]] = process_options{})
    {
        assert(!std::empty(path) && "nes::process::process called with empty path.");

        SECURITY_ATTRIBUTES security_attributes{};
        security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        security_attributes.bInheritHandle = TRUE;
        security_attributes.lpSecurityDescriptor = nullptr;

    #ifdef NES_PROCESS_PIPE_EXTENSION
        impl::auto_handle stdin_rd{};
        impl::auto_handle stdout_rd{};
        impl::auto_handle stderr_rd{};
        impl::auto_handle stdin_wr{};
        impl::auto_handle stdout_wr{};
        impl::auto_handle stderr_wr{};

        if(static_cast<bool>(options & process_options::grab_stdin))
            if(!CreatePipe(&stdin_rd, &stdin_wr, &security_attributes, 0) || !SetHandleInformation(stdin_wr, HANDLE_FLAG_INHERIT, 0))
                throw std::runtime_error{"Failed to create stdin pipe. " + get_error_message()};

        if(static_cast<bool>(options & process_options::grab_stdout))
            if(!CreatePipe(&stdout_rd, &stdout_wr, &security_attributes, 0) || !SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0))
                throw std::runtime_error{"Failed to create stdout pipe. " + get_error_message()};

        if(static_cast<bool>(options & process_options::grab_stderr))
            if(!CreatePipe(&stderr_rd, &stderr_wr, &security_attributes, 0) || !SetHandleInformation(stderr_rd, HANDLE_FLAG_INHERIT, 0))
                throw std::runtime_error{"Failed to create stderr pipe. " + get_error_message()};
    #endif

        args.insert(std::begin(args), path);

        auto format_arg = [](const std::wstring& arg) -> std::wstring
        {
            if(arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
                return arg;

            std::wstring out{L"\""};
            for(auto it = std::cbegin(arg); it != std::cend(arg); ++it)
            {
                if(*it == L'\\')
                {
                    std::size_t count{1};
                    while(++it != std::cend(arg) && *it == L'\\')
                        ++count;

                    if(it == std::cend(arg))
                    {
                        out.append(count * 2, L'\\');
                        break;
                    }
                    else if(*it == L'\"')
                    {
                        out.append(count * 2 + 1, L'\\');
                        out.push_back(L'\"');
                    }
                    else
                    {
                        out.append(count, L'\\');
                        out.push_back(*it);
                    }
                }
                else if(*it == L'\"')
                {
                    out.push_back(L'\\');
                    out.push_back(*it);
                }
                else
                {
                    out.push_back(*it);
                }
            }
            out.push_back(L'\"');

            return out;
        };

        std::wstring args_str{};
        for(auto&& arg : args)
            args_str += format_arg(to_wide(arg)) + L" ";

        const std::wstring native_working_directory{to_wide(working_directory)};
        const std::wstring native_path{to_wide(path)};

        STARTUPINFOW startup_info{};
        startup_info.cb = sizeof(STARTUPINFOW);
    #ifdef NES_PROCESS_PIPE_EXTENSION
        startup_info.hStdInput = stdin_rd;
        startup_info.hStdOutput = stdout_wr;
        startup_info.hStdError = stderr_wr;
        if(static_cast<std::uint32_t>(options) != 0)
            startup_info.dwFlags = STARTF_USESTDHANDLES;
    #endif

        PROCESS_INFORMATION process_info{};
        if(!CreateProcessW(std::data(native_path), null_or_data(args_str), nullptr, nullptr, TRUE, 0, nullptr, null_or_data(native_working_directory), &startup_info, &process_info))
            throw std::runtime_error{"Failed to create process. " + get_error_message()};

        m_id = static_cast<id>(process_info.dwProcessId);
        m_handle = process_info.hProcess;
        m_thread_handle = process_info.hThread;

    #ifdef NES_PROCESS_PIPE_EXTENSION
        if(static_cast<bool>(options & process_options::grab_stdin))
        {
            pipe_streambuf buffer{stdin_wr.release(), std::ios_base::out};
            m_stdin_stream.reset(new pipe_ostream{std::move(buffer)});
        }

        if(static_cast<bool>(options & process_options::grab_stdout))
        {
            pipe_streambuf buffer{stdout_rd.release(), std::ios_base::in};
            m_stdout_stream.reset(new pipe_istream{std::move(buffer)});
        }

        if(static_cast<bool>(options & process_options::grab_stderr))
        {
            pipe_streambuf buffer{stderr_rd.release(), std::ios_base::in};
            m_stderr_stream.reset(new pipe_istream{std::move(buffer)});
        }
    #endif
    }

    ~process()
    {
        assert(!joinable() && "nes::process::~process() called with joinable() returning true.");

        if(joinable())
            std::terminate();
    }

    process(const process&) = delete;
    process& operator=(const process&) = delete;

    process(process&& other) noexcept
    :m_id{std::exchange(other.m_id, id{})}
    ,m_return_code{std::exchange(other.m_return_code, return_code_type{})}
    ,m_handle{std::move(other.m_handle)}
    ,m_thread_handle{std::move(other.m_thread_handle)}
#ifdef NES_PROCESS_PIPE_EXTENSION
    ,m_stdin_stream{std::move(other.m_stdin_stream)}
    ,m_stdout_stream{std::move(other.m_stdout_stream)}
    ,m_stderr_stream{std::move(other.m_stderr_stream)}
#endif
    {

    }

    process& operator=(process&& other) noexcept
    {
        if(joinable())
            std::terminate();

        m_id = std::exchange(other.m_id, m_id);
        m_return_code = std::exchange(other.m_return_code, m_return_code);
        m_handle = std::move(other.m_handle);
        m_thread_handle = std::move(other.m_thread_handle);
    #ifdef NES_PROCESS_PIPE_EXTENSION
        m_stdin_stream = std::move(other.m_stdin_stream);
        m_stdout_stream = std::move(other.m_stdout_stream);
        m_stderr_stream = std::move(other.m_stderr_stream);
    #endif

        return *this;
    }

    void join()
    {
        assert(joinable() && "nes::process::join() called with joinable() returning false.");

        if(WaitForSingleObject(m_handle, INFINITE))
            throw std::runtime_error{"Failed to join the process. " + get_error_message()};

        if(!GetExitCodeProcess(m_handle, reinterpret_cast<DWORD*>(&m_return_code)))
            throw std::runtime_error{"Failed to get the return code of the process. " + get_error_message()};

        close_process();
    }

    bool joinable() const noexcept
    {
        return m_handle;
    }

    bool active() const
    {
        if(!m_handle)
            return false;

        DWORD result = WaitForSingleObject(m_handle, 0);
        if(result == WAIT_FAILED)
            throw std::runtime_error{"Failed to get the state of the process. " + get_error_message()};

        return result == WAIT_TIMEOUT;
    }

    void detach()
    {
        assert(joinable() && "nes::process::detach() called with joinable() returning false.");

        close_process();
    }

    bool kill()
    {
        assert(joinable() && "nes::process::kill() called with joinable() returning false.");

        if(!TerminateProcess(m_handle, 1))
            return false;

        join();

        return true;
    }

    return_code_type return_code() const noexcept
    {
        assert(!joinable() && "nes::process::return_code() called with joinable() returning true.");

        return m_return_code;
    }

    native_handle_type native_handle() const noexcept
    {
        return m_handle;
    }

    id get_id() const noexcept
    {
        return m_id;
    }

#ifdef NES_PROCESS_PIPE_EXTENSION
    pipe_ostream& stdin_stream() noexcept
    {
        return *m_stdin_stream;
    }

    pipe_istream& stdout_stream() noexcept
    {
        return *m_stdout_stream;
    }

    pipe_istream& stderr_stream() noexcept
    {
        return *m_stderr_stream;
    }
#endif

private:
    std::wstring to_wide(const std::string& path)
    {
        assert(std::size(path) < 0x7FFFFFFFu && "Wrong path.");

        if(std::empty(path))
            return {};

        std::wstring out_path{};
        out_path.resize(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), nullptr, 0)));

        if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), std::data(out_path), static_cast<int>(std::size(out_path))))
            throw std::runtime_error{"Failed to convert the path to wide."};

        return out_path;
    }

    std::string get_error_message() const
    {
        return "#" + std::to_string(GetLastError());
    }

    void close_process()
    {
        m_id = id{};
        CloseHandle(m_handle.release());
        CloseHandle(m_thread_handle.release());
    }

    wchar_t* null_or_data(std::wstring& str)
    {
        return std::empty(str) ? nullptr : std::data(str);
    }

    const wchar_t* null_or_data(const std::wstring& str)
    {
        return std::empty(str) ? nullptr : std::data(str);
    }

private:
    id m_id{};
    return_code_type m_return_code{};
    impl::auto_handle m_handle{};
    impl::auto_handle m_thread_handle{};
#ifdef NES_PROCESS_PIPE_EXTENSION
    std::unique_ptr<pipe_ostream> m_stdin_stream{};
    std::unique_ptr<pipe_istream> m_stdout_stream{};
    std::unique_ptr<pipe_istream> m_stderr_stream{};
#endif
};

namespace this_process
{

inline process::id get_id() noexcept
{
    return process::id{GetCurrentProcessId()};
}

inline std::string working_directory()
{
    const DWORD size{GetCurrentDirectoryW(0, nullptr)};

    std::wstring native_path{};
    native_path.resize(static_cast<std::size_t>(size));
    GetCurrentDirectoryW(size, std::data(native_path));
    native_path.pop_back(); //Because GetCurrentDirectoryW adds a null terminator

    std::transform(std::begin(native_path), std::end(native_path), std::begin(native_path), [](wchar_t c){return c == L'\\' ? L'/' : c;});

    std::string path{};
    path.resize(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, std::data(native_path), static_cast<int>(std::size(native_path)), nullptr, 0, nullptr, nullptr)));

    if(!WideCharToMultiByte(CP_UTF8, 0, std::data(native_path), static_cast<int>(std::size(native_path)), std::data(path), static_cast<int>(std::size(path)), nullptr, nullptr))
        throw std::runtime_error{"Failed to convert the path to UTF-8."};

    return path;
}

inline bool change_working_directory(const std::string& path)
{
    std::wstring native_path{};
    native_path.resize(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), nullptr, 0)));

    if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), std::data(native_path), static_cast<int>(std::size(native_path))))
        throw std::runtime_error{"Failed to convert the path to wide."};

    return SetCurrentDirectoryW(std::data(native_path));
}

}

}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, nes::process::id id)
{
    return os << static_cast<DWORD>(id);
}

namespace std
{
    template<>
    struct hash<nes::process::id>
    {
        using argument_type = nes::process::id;
        using result_type = std::size_t;

        result_type operator()(const argument_type& s) const noexcept
        {
            return std::hash<DWORD>{}(static_cast<DWORD>(s));
        }
    };
}

#elif defined(NES_POSIX_PROCESS)

namespace nes
{

class process;

namespace impl
{

enum class id_t : pid_t{};

constexpr bool operator==(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) == static_cast<pid_t>(rhs);
}

constexpr bool operator!=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) != static_cast<pid_t>(rhs);
}

constexpr bool operator<(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) < static_cast<pid_t>(rhs);
}

constexpr bool operator<=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) <= static_cast<pid_t>(rhs);
}

constexpr bool operator>(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) > static_cast<pid_t>(rhs);
}

constexpr bool operator>=(id_t lhs, id_t rhs) noexcept
{
    return static_cast<pid_t>(lhs) >= static_cast<pid_t>(rhs);
}

#ifdef NES_PROCESS_PIPE_EXTENSION
struct auto_handle
{
    constexpr auto_handle() = default;
    auto_handle(int h)
    :m_handle{h}
    {

    }

    ~auto_handle()
    {
        if(m_handle)
            close(m_handle);
    }

    auto_handle(const auto_handle&) = delete;
    auto_handle& operator=(const auto_handle&) = delete;

    auto_handle(auto_handle&& other) noexcept
    :m_handle{std::exchange(other.m_handle, -1)}
    {

    }

    auto_handle& operator=(auto_handle&& other) noexcept
    {
        m_handle = std::exchange(other.m_handle, m_handle);

        return *this;
    }

    int release() noexcept
    {
        return std::exchange(m_handle, -1);
    }

    operator int() noexcept
    {
        return m_handle;
    }

    operator int() const noexcept
    {
        return m_handle;
    }

    int* operator&() noexcept
    {
        return &m_handle;
    }

    const int* operator&() const noexcept
    {
        return &m_handle;
    }

    operator bool() const noexcept
    {
        return m_handle != -1;
    }

    int m_handle{-1};
};
#endif

}

enum class process_options : std::uint32_t
{
    none = 0x00,
#ifdef NES_PROCESS_PIPE_EXTENSION
    grab_stdout = 0x10,
    grab_stderr = 0x20,
    grab_stdin = 0x40
#endif
};

constexpr process_options operator&(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr process_options& operator&=(process_options& left, process_options right) noexcept
{
    left = left & right;
    return left;
}

constexpr process_options operator|(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr process_options& operator|=(process_options& left, process_options right) noexcept
{
    left = left | right;
    return left;
}

constexpr process_options operator^(process_options left, process_options right) noexcept
{
    return static_cast<process_options>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}

constexpr process_options& operator^=(process_options& left, process_options right) noexcept
{
    left = left ^ right;
    return left;
}

constexpr process_options operator~(process_options value) noexcept
{
    return static_cast<process_options>(~static_cast<std::uint32_t>(value));
}

class process
{
public:
    using native_handle_type = pid_t;
    using return_code_type = int;
    using id = impl::id_t;

public:
    constexpr process() noexcept = default;

    explicit process(const std::string& path, const std::string& working_directory)
    :process{path, {}, working_directory, {}}{}

    explicit process(const std::string& path, process_options options)
    :process{path, {}, {}, options}{}

    explicit process(const std::string& path, const std::vector<std::string>& args, process_options options)
    :process{path, args, {}, options}{}

    explicit process(const std::string& path, const std::string& working_directory, process_options options)
    :process{path, {}, working_directory, options}{}

    explicit process(const std::string& path, std::vector<std::string> args = std::vector<std::string>{}, const std::string& working_directory = std::string{}, process_options options [[maybe_unused]] = process_options{})
    {
        assert(!std::empty(path) && "nes::process::process called with empty path.");

        args.insert(std::begin(args), path);

        std::vector<char*> native_args{};
        native_args.resize(std::size(args) + 1);
        for(std::size_t i{}; i < std::size(args); ++i)
            native_args[i] = std::data(args[i]);

    #ifdef NES_PROCESS_PIPE_EXTENSION
        impl::auto_handle stdin_fd[2]{};
        impl::auto_handle stdout_fd[2]{};
        impl::auto_handle stderr_fd[2]{};

        if(static_cast<bool>(options & process_options::grab_stdin) && pipe(reinterpret_cast<int*>(stdin_fd)))
            throw std::runtime_error{"Failed to create stdin pipe. " + std::string{strerror(errno)}};

        if(static_cast<bool>(options & process_options::grab_stdout) && pipe(reinterpret_cast<int*>(stdout_fd)))
            throw std::runtime_error{"Failed to create stdout pipe. " + std::string{strerror(errno)}};

        if(static_cast<bool>(options & process_options::grab_stderr) && pipe(reinterpret_cast<int*>(stderr_fd)))
            throw std::runtime_error{"Failed to create stderr pipe. " + std::string{strerror(errno)}};

        const bool standard_streams{static_cast<bool>(options & process_options::grab_stdin) || static_cast<bool>(options & process_options::grab_stdout) || static_cast<bool>(options & process_options::grab_stderr)};
    #else
        constexpr bool standard_streams{false};
    #endif

        const pid_t id{fork()};
        if(id < 0)
        {
            throw std::runtime_error{"Failed to create process. " + std::string{strerror(errno)}};
        }
        else if(id == 0)
        {

        #ifdef NES_PROCESS_PIPE_EXTENSION
            if(static_cast<bool>(options & process_options::grab_stdin))
                if(dup2(stdin_fd[0], 0) == -1)
                    _exit(EXIT_FAILURE);

            if(static_cast<bool>(options & process_options::grab_stdout))
                if(dup2(stdout_fd[1], 1) == -1)
                    _exit(EXIT_FAILURE);

            if(static_cast<bool>(options & process_options::grab_stderr))
                if(dup2(stderr_fd[1], 2) == -1)
                    _exit(EXIT_FAILURE);
        #endif

            if(!std::empty(working_directory))
                if(chdir(std::data(working_directory)))
                    _exit(EXIT_FAILURE);

            execv(std::data(path), std::data(native_args));
            _exit(EXIT_FAILURE);
        }

        m_id = id;

    #ifdef NES_PROCESS_PIPE_EXTENSION
        if(static_cast<bool>(options & process_options::grab_stdin))
             m_stdin_stream.reset(new pipe_ostream{pipe_streambuf{stdin_fd[1].release(), std::ios_base::out}});

        if(static_cast<bool>(options & process_options::grab_stdout))
            m_stdout_stream.reset(new pipe_istream{pipe_streambuf{stdout_fd[0].release(), std::ios_base::in}});

        if(static_cast<bool>(options & process_options::grab_stderr))
            m_stderr_stream.reset(new pipe_istream{pipe_streambuf{stderr_fd[0].release(), std::ios_base::in}});
    #endif
    }

    ~process()
    {
        assert(!joinable() && "nes::process::~process() called with joinable() returning true.");

        if(joinable())
            std::terminate();
    }

    process(const process&) = delete;
    process& operator=(const process&) = delete;

    process(process&& other) noexcept
    :m_id{std::exchange(other.m_id, -1)}
    ,m_return_code{std::exchange(other.m_return_code, return_code_type{})}
#ifdef NES_PROCESS_PIPE_EXTENSION
    ,m_stdin_stream{std::move(other.m_stdin_stream)}
    ,m_stdout_stream{std::move(other.m_stdout_stream)}
    ,m_stderr_stream{std::move(other.m_stderr_stream)}
#endif
    {

    }

    process& operator=(process&& other) noexcept
    {
        if(joinable())
            std::terminate();

        m_id = std::exchange(other.m_id, m_id);
        m_return_code = std::exchange(other.m_return_code, m_return_code);
    #ifdef NES_PROCESS_PIPE_EXTENSION
        m_stdin_stream = std::move(other.m_stdin_stream);
        m_stdout_stream = std::move(other.m_stdout_stream);
        m_stderr_stream = std::move(other.m_stderr_stream);
    #endif

        return *this;
    }

    void join()
    {
        assert(joinable() && "nes::process::join() called with joinable() returning false.");

        int return_code{};
        if(waitpid(m_id, &return_code, 0) == -1)
            throw std::runtime_error{"Failed to join the process. " + std::string{strerror(errno)}};

        m_id = -1;
        m_return_code = WEXITSTATUS(return_code);
    }

    bool joinable() const noexcept
    {
        return m_id != -1;
    }

    bool active() const
    {
        return ::kill(m_id, 0) != ESRCH;
    }

    void detach()
    {
        assert(joinable() && "nes::process::detach() called with joinable() returning false.");
        m_id = -1;
    }

    bool kill()
    {
        assert(joinable() && "nes::process::kill() called with joinable() returning false.");

        if(::kill(m_id, SIGTERM))
            return false;

        join();

        return true;
    }

    return_code_type return_code() const noexcept
    {
        assert(!joinable() && "nes::process::return_code() called with joinable() returning true.");

        return m_return_code;
    }

    native_handle_type native_handle() const noexcept
    {
        return m_id;
    }

    id get_id() const noexcept
    {
        return static_cast<impl::id_t>(m_id);
    }

#ifdef NES_PROCESS_PIPE_EXTENSION
    pipe_ostream& stdin_stream() noexcept
    {
        return *m_stdin_stream;
    }

    pipe_istream& stdout_stream() noexcept
    {
        return *m_stdout_stream;
    }

    pipe_istream& stderr_stream() noexcept
    {
        return *m_stderr_stream;
    }
#endif

private:
    native_handle_type m_id{};
    return_code_type m_return_code{};
#ifdef NES_PROCESS_PIPE_EXTENSION
    std::unique_ptr<pipe_ostream> m_stdin_stream{};
    std::unique_ptr<pipe_istream> m_stdout_stream{};
    std::unique_ptr<pipe_istream> m_stderr_stream{};
#endif
};

namespace this_process
{

inline process::id get_id() noexcept
{
    return process::id{getpid()};
}

inline std::string working_directory()
{
    std::string path{};
    path.resize(256);

    while(!getcwd(std::data(path), std::size(path)))
    {
        if(errno == ERANGE)
            path.resize(std::size(path) * 2);
        else
            throw std::runtime_error{"Failed to get the current working directory. " + std::string{strerror(errno)}};
    }

    path.resize(path.find_first_of('\0'));

    return path;
}

inline bool change_working_directory(const std::string& path)
{
    return chdir(std::data(path)) == 0;
}

}

}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, nes::process::id id)
{
    return os << static_cast<int>(id);
}

namespace std
{
    template<>
    struct hash<nes::process::id>
    {
        using argument_type = nes::process::id;
        using result_type = std::size_t;

        result_type operator()(const argument_type& s) const noexcept
        {
            return std::hash<int>{}(static_cast<int>(s));
        }
    };
}


#endif

#endif
