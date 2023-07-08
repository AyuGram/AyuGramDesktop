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

#ifndef NOT_ENOUGH_STANDARDS_PIPE
#define NOT_ENOUGH_STANDARDS_PIPE

#if defined(_WIN32)
    #define NES_WIN32_PIPE
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #define NES_POSIX_PIPE
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
#else
    #error "Not enough standards does not support this environment."
#endif

#include <vector>
#include <algorithm>
#include <streambuf>
#include <istream>
#include <ostream>
#include <memory>
#include <cassert>
#include <utility>

#if defined(NES_WIN32_PIPE)

namespace nes
{

inline constexpr const char pipe_root[] = "\\\\.\\pipe\\";

template<typename CharT, typename Traits>
class basic_pipe_istream;
template<typename CharT, typename Traits>
class basic_pipe_ostream;
template<typename CharT = char, typename Traits = std::char_traits<CharT>>
std::pair<basic_pipe_istream<CharT, Traits>, basic_pipe_ostream<CharT, Traits>> make_anonymous_pipe();

template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_streambuf : public std::basic_streambuf<CharT, Traits>
{
private:
    using parent_type = std::basic_streambuf<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    static constexpr std::size_t buf_size{1024};

public:
    basic_pipe_streambuf() = default;

    explicit basic_pipe_streambuf(const std::string& name, std::ios_base::openmode mode)
    {
        open(name, mode);
    }

    virtual ~basic_pipe_streambuf()
    {
        close();
    }

    basic_pipe_streambuf(const basic_pipe_streambuf&) = delete;
    basic_pipe_streambuf& operator=(const basic_pipe_streambuf&) = delete;

    basic_pipe_streambuf(basic_pipe_streambuf&& other) noexcept
    :parent_type{other}
    ,m_buffer{std::move(other.m_buffer)}
    ,m_handle{std::exchange(other.m_handle, INVALID_HANDLE_VALUE)}
    ,m_mode{std::exchange(other.m_mode, std::ios_base::openmode{})}
    {

    }

    basic_pipe_streambuf& operator=(basic_pipe_streambuf&& other) noexcept
    {
        parent_type::operator=(other);
        m_buffer = std::move(other.m_buffer);
        m_handle = std::exchange(other.m_handle, m_handle);
        m_mode = std::exchange(other.m_mode, m_mode);

        return *this;
    }

    bool open(const std::string& name, std::ios_base::openmode mode)
    {
        assert(!((mode & std::ios_base::in) && (mode & std::ios_base::out)) && "nes::basic_pipe_streambuf::open called with mode = std::ios_base::in | std::ios_base::out.");

        close();

        const auto native_name{to_wide(pipe_root + name)};
        DWORD native_mode{mode & std::ios_base::in ? GENERIC_READ : GENERIC_WRITE};

        HANDLE handle = CreateFileW(std::data(native_name), native_mode, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if(handle == INVALID_HANDLE_VALUE)
        {
            if(GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                native_mode = mode & std::ios_base::in ? PIPE_ACCESS_INBOUND : PIPE_ACCESS_OUTBOUND;

                handle = CreateNamedPipeW(std::data(native_name), native_mode, PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, buf_size, buf_size, 0, nullptr);
                if(handle == INVALID_HANDLE_VALUE)
                    return false;

                if(!ConnectNamedPipe(handle, nullptr))
                {
                    CloseHandle(handle);
                    return false;
                }
            }
        }

        m_buffer.resize(buf_size);
        parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        m_handle = handle;
        m_mode = mode;

        return true;
    }

    bool is_open() const noexcept
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    void close()
    {
        if(is_open())
        {
            sync();

            m_mode = std::ios_base::openmode{};
            CloseHandle(std::exchange(m_handle, INVALID_HANDLE_VALUE));
            parent_type::setp(nullptr, nullptr);
            parent_type::setg(nullptr, nullptr, nullptr);
        }
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_streambuf(HANDLE handle, std::ios_base::openmode mode)
    :m_handle{handle}
    ,m_mode{mode}
    {
        m_buffer.resize(buf_size);
        parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
    }

protected:
    virtual int sync() override
    {
        if(m_mode & std::ios_base::out)
        {
            const std::ptrdiff_t count{parent_type::pptr() - parent_type::pbase()};

            DWORD written{};
            if(!WriteFile(m_handle, reinterpret_cast<const CHAR*>(std::data(m_buffer)), static_cast<DWORD>(count)* sizeof(char_type), &written, nullptr))
                return -1;

            parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        }

        return 0;
    }

    virtual int_type overflow(int_type c = traits_type::eof()) override
    {
        assert(m_mode & std::ios_base::out && "Write operation on a read only pipe.");

        if(traits_type::eq_int_type(c, traits_type::eof()))
        {
            DWORD written{};
            if(!WriteFile(m_handle, reinterpret_cast<const CHAR*>(std::data(m_buffer)), static_cast<DWORD>(buf_size) * sizeof(char_type), &written, nullptr))
                return traits_type::eof();

            parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        }
        else
        {
            *parent_type::pptr() = traits_type::to_char_type(c);
            parent_type::pbump(1);
        }

        return traits_type::not_eof(c);
    }

    virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
    {
        assert(m_mode & std::ios_base::out && "Write operation on a read only pipe.");

        DWORD written{};
        if(!WriteFile(m_handle, reinterpret_cast<const CHAR*>(s), static_cast<DWORD>(count) * sizeof(char_type), &written, nullptr))
            return 0;

        return static_cast<std::streamsize>(written);
    }

    virtual int_type underflow() override
    {
        assert(m_mode & std::ios_base::in && "Read operation on a write only pipe.");

        if(parent_type::gptr() == parent_type::egptr())
        {
            DWORD readed{};
            if(!ReadFile(m_handle, reinterpret_cast<CHAR*>(std::data(m_buffer)), static_cast<DWORD>(buf_size * sizeof(char_type)), &readed, nullptr) || readed == 0)
                return traits_type::eof();

            parent_type::setg(std::data(m_buffer), std::data(m_buffer), std::data(m_buffer) + (readed / sizeof(char_type)));
        }

        return traits_type::to_int_type(*parent_type::gptr());
    }

    virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override
    {
        assert(m_mode & std::ios_base::in && "Read operation on a write only pipe.");

        DWORD readed{};
        if(!ReadFile(m_handle, reinterpret_cast<CHAR*>(s), static_cast<DWORD>(count) * sizeof(char_type), &readed, nullptr))
            return 0;

        return static_cast<std::streamsize>(readed / sizeof(char_type));
    }

private:
    std::wstring to_wide(std::string path)
    {
        assert(std::size(path) < 0x7FFFFFFFu && "Wrong path.");

        if(std::empty(path))
            return {};

        std::transform(std::begin(path), std::end(path), std::begin(path), [](char c){return c == '/' ? '\\' : c;});

        std::wstring out_path{};
        out_path.resize(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), nullptr, 0)));

        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, std::data(path), static_cast<int>(std::size(path)), std::data(out_path), static_cast<int>(std::size(out_path))))
            throw std::runtime_error{"Failed to convert the path to wide."};

        return out_path;
    }

private:
    std::vector<CharT> m_buffer{};
    HANDLE m_handle{INVALID_HANDLE_VALUE};
    std::ios_base::openmode m_mode{};
};


template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_istream : public std::basic_istream<CharT, Traits>
{
private:
    using parent_type = std::basic_istream<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    basic_pipe_istream() = default;

    explicit basic_pipe_istream(const std::string& name, std::ios_base::openmode mode = std::ios_base::in)
    :parent_type{nullptr}
    {
        parent_type::rdbuf(m_buffer.get());
        open(name, mode);
    }

    virtual ~basic_pipe_istream() = default;

    basic_pipe_istream(const basic_pipe_istream&) = delete;
    basic_pipe_istream& operator=(const basic_pipe_istream&) = delete;

    basic_pipe_istream(basic_pipe_istream&& other) noexcept
    :parent_type{std::move(other)}
    {
        std::swap(m_buffer, other.m_buffer);
        parent_type::rdbuf(m_buffer.get());
    }

    basic_pipe_istream& operator=(basic_pipe_istream&& other) noexcept
    {
        parent_type::operator=(std::move(other));
        std::swap(m_buffer, other.m_buffer);

        parent_type::rdbuf(m_buffer.get());

        return *this;
    }

    void open(const std::string& name, std::ios_base::openmode mode = std::ios_base::in)
    {
        m_buffer->open(name, mode);
        parent_type::clear(m_buffer->is_open() ? std::ios_base::goodbit : std::ios_base::failbit);
    }

    bool is_open() const noexcept
    {
        return m_buffer->is_open();
    }

    void close()
    {
        m_buffer->close();
    }

    basic_pipe_streambuf<char_type, traits_type>* rdbuf() const noexcept
    {
        return m_buffer.get();
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_istream(basic_pipe_streambuf<char_type, traits_type> buffer)
    :parent_type{nullptr}
    ,m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>(std::move(buffer))}
    {
        parent_type::rdbuf(m_buffer.get());
    }

private:
    std::unique_ptr<basic_pipe_streambuf<char_type, traits_type>> m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>()};
};

template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_ostream : public std::basic_ostream<CharT, Traits>
{
private:
    using parent_type = std::basic_ostream<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    basic_pipe_ostream() = default;

    explicit basic_pipe_ostream(const std::string& name, std::ios_base::openmode mode = std::ios_base::out)
    :parent_type{nullptr}
    {
        parent_type::rdbuf(m_buffer.get());
        open(name, mode);
    }

    virtual ~basic_pipe_ostream() = default;

    basic_pipe_ostream(const basic_pipe_ostream&) = delete;
    basic_pipe_ostream& operator=(const basic_pipe_ostream&) = delete;

    basic_pipe_ostream(basic_pipe_ostream&& other) noexcept
    :parent_type{std::move(other)}
    {
        std::swap(m_buffer, other.m_buffer);
        parent_type::rdbuf(m_buffer.get());
    }

    basic_pipe_ostream& operator=(basic_pipe_ostream&& other) noexcept
    {
        parent_type::operator=(std::move(other));
        std::swap(m_buffer, other.m_buffer);

        parent_type::rdbuf(m_buffer.get());

        return *this;
    }

    void open(const std::string& name, std::ios_base::openmode mode = std::ios_base::out)
    {
        m_buffer->open(name, mode);
        parent_type::clear(m_buffer->is_open() ? std::ios_base::goodbit : std::ios_base::failbit);
    }

    bool is_open() const noexcept
    {
        return m_buffer->is_open();
    }

    void close()
    {
        m_buffer->close();
    }

    basic_pipe_streambuf<char_type, traits_type>* rdbuf() const noexcept
    {
        return m_buffer.get();
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_ostream(basic_pipe_streambuf<char_type, traits_type> buffer)
    :parent_type{nullptr}
    ,m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>(std::move(buffer))}
    {
        parent_type::rdbuf(m_buffer.get());
    }

private:
    std::unique_ptr<basic_pipe_streambuf<char_type, traits_type>> m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>()};
};

template<typename CharT, typename Traits>
std::pair<basic_pipe_istream<CharT, Traits>, basic_pipe_ostream<CharT, Traits>> make_anonymous_pipe()
{
    HANDLE input{};
    HANDLE output{};

    if(!CreatePipe(&input, &output, nullptr, 0))
        throw std::runtime_error{"Failed to create pipe"};

    return std::make_pair(basic_pipe_istream<CharT, Traits>{basic_pipe_streambuf<CharT, Traits>{input, std::ios_base::in}},
                          basic_pipe_ostream<CharT, Traits>{basic_pipe_streambuf<CharT, Traits>{output, std::ios_base::out}});
}

using pipe_streambuf = basic_pipe_streambuf<char>;
using pipe_istream = basic_pipe_istream<char>;
using pipe_ostream = basic_pipe_ostream<char>;

}

#elif defined(NES_POSIX_PIPE)


namespace nes
{

inline constexpr const char pipe_root[] = "/tmp/";

template<typename CharT, typename Traits>
class basic_pipe_istream;
template<typename CharT, typename Traits>
class basic_pipe_ostream;
template<typename CharT = char, typename Traits = std::char_traits<CharT>>
std::pair<basic_pipe_istream<CharT, Traits>, basic_pipe_ostream<CharT, Traits>> make_anonymous_pipe();

template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_streambuf : public std::basic_streambuf<CharT, Traits>
{
private:
    using parent_type = std::basic_streambuf<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    static constexpr std::size_t buf_size{1024};

public:
    basic_pipe_streambuf() = default;

    explicit basic_pipe_streambuf(const std::string& name, std::ios_base::openmode mode)
    :parent_type{nullptr}
    {
        open(name, mode);
    }

    virtual ~basic_pipe_streambuf()
    {
        close();
    }

    basic_pipe_streambuf(const basic_pipe_streambuf&) = delete;
    basic_pipe_streambuf& operator=(const basic_pipe_streambuf&) = delete;

    basic_pipe_streambuf(basic_pipe_streambuf&& other) noexcept
    :parent_type{std::move(other)}
    ,m_buffer{std::move(other.m_buffer)}
    ,m_handle{std::exchange(other.m_handle, 0)}
    ,m_mode{std::exchange(other.m_mode, std::ios_base::openmode{})}
    {

    }

    basic_pipe_streambuf& operator=(basic_pipe_streambuf&& other) noexcept
    {
        parent_type::operator=(std::move(other));
        m_buffer = std::move(other.m_buffer);
        m_handle = std::exchange(other.m_handle, m_handle);
        m_mode = std::exchange(other.m_mode, m_mode);

        return *this;
    }

    bool open(const std::string& name, std::ios_base::openmode mode)
    {
        assert(!((mode & std::ios_base::in) && (mode & std::ios_base::out)) && "nes::basic_pipe_streambuf::open called with mode = std::ios_base::in | std::ios_base::out.");

        close();

        const auto native_name{pipe_root + name};
        if(mkfifo(std::data(native_name), 0660) != 0 && errno != EEXIST)
            return false;

        const int native_mode{mode & std::ios_base::in ? O_RDONLY : O_WRONLY};
        int handle = ::open(std::data(native_name), native_mode);
        if(handle < 0)
            return false;

        m_buffer.resize(buf_size);
        parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        m_handle = handle;
        m_mode = mode;

        return true;
    }

    bool is_open() const noexcept
    {
        return m_handle;
    }

    void close()
    {
        if(is_open())
        {
            sync();

            m_mode = std::ios_base::openmode{};
            ::close(std::exchange(m_handle, 0));
            parent_type::setp(nullptr, nullptr);
            parent_type::setg(nullptr, nullptr, nullptr);
        }
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_streambuf(int handle, std::ios_base::openmode mode)
    :m_handle{handle}
    ,m_mode{mode}
    {
        m_buffer.resize(buf_size);
        parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
    }

protected:
    virtual int sync() override
    {
        if(m_mode & std::ios_base::out)
        {
            const std::ptrdiff_t count{parent_type::pptr() - parent_type::pbase()};

            if(write(m_handle, reinterpret_cast<const char*>(std::data(m_buffer)), count * sizeof(char_type)) < 0)
                return -1;

            parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        }

        return 0;
    }

    virtual int_type overflow(int_type c = traits_type::eof()) override
    {
        assert(m_mode & std::ios_base::out && "Write operation on a read only pipe.");

        if(traits_type::eq_int_type(c, traits_type::eof()))
        {
            if(write(m_handle, reinterpret_cast<const char*>(std::data(m_buffer)), std::size(m_buffer) * sizeof(char_type)))
                return traits_type::eof();

            parent_type::setp(std::data(m_buffer), std::data(m_buffer) + buf_size);
        }
        else
        {
            *parent_type::pptr() = traits_type::to_char_type(c);
            parent_type::pbump(1);
        }

        return traits_type::not_eof(c);
    }

    virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
    {
        assert(m_mode & std::ios_base::out && "Write operation on a read only pipe.");

        const auto written = write(m_handle, reinterpret_cast<const char*>(s), count * sizeof(char_type));
        if(written < 0)
            return 0;

        return static_cast<std::streamsize>(written);
    }

    virtual int_type underflow() override
    {
        assert(m_mode & std::ios_base::in && "Read operation on a write only pipe.");

        if(parent_type::gptr() == parent_type::egptr())
        {
            const auto readed = read(m_handle, reinterpret_cast<char*>(std::data(m_buffer)), buf_size * sizeof(char_type));
            if(readed <= 0)
                return traits_type::eof();

            parent_type::setg(std::data(m_buffer), std::data(m_buffer), std::data(m_buffer) + (readed / sizeof(char_type)));
        }

        return traits_type::to_int_type(*parent_type::gptr());
    }

    virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override
    {
        assert(m_mode & std::ios_base::in && "Read operation on a write only pipe.");

        const auto readed = read(m_handle, reinterpret_cast<char*>(s), count * sizeof(char_type));
        if(readed < 0)
            return 0;

        return static_cast<std::streamsize>(readed / sizeof(char_type));
    }

private:
    std::vector<CharT> m_buffer{};
    int m_handle{};
    std::ios_base::openmode m_mode{};
};

template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_istream : public std::basic_istream<CharT, Traits>
{
private:
    using parent_type = std::basic_istream<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    basic_pipe_istream() = default;

    explicit basic_pipe_istream(const std::string& name, std::ios_base::openmode mode = std::ios_base::in)
    :parent_type{nullptr}
    {
        parent_type::rdbuf(m_buffer.get());
        open(name, mode);
    }

    virtual ~basic_pipe_istream() = default;

    basic_pipe_istream(const basic_pipe_istream&) = delete;
    basic_pipe_istream& operator=(const basic_pipe_istream&) = delete;

    basic_pipe_istream(basic_pipe_istream&& other) noexcept
    :parent_type{std::move(other)}
    {
        std::swap(m_buffer, other.m_buffer);
        parent_type::rdbuf(m_buffer.get());
    }

    basic_pipe_istream& operator=(basic_pipe_istream&& other) noexcept
    {
        parent_type::operator=(std::move(other));
        std::swap(m_buffer, other.m_buffer);

        parent_type::rdbuf(m_buffer.get());

        return *this;
    }

    void open(const std::string& name, std::ios_base::openmode mode = std::ios_base::in)
    {
        m_buffer->open(name, mode);
        parent_type::clear(m_buffer->is_open() ? std::ios_base::goodbit : std::ios_base::failbit);
    }

    bool is_open() const noexcept
    {
        return m_buffer->is_open();
    }

    void close()
    {
        m_buffer->close();
    }

    basic_pipe_streambuf<char_type, traits_type>* rdbuf() const noexcept
    {
        return m_buffer.get();
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_istream(basic_pipe_streambuf<char_type, traits_type> buffer)
    :parent_type{nullptr}
    ,m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>(std::move(buffer))}
    {
        parent_type::rdbuf(m_buffer.get());
    }

private:
    std::unique_ptr<basic_pipe_streambuf<char_type, traits_type>> m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>()};
};

template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_pipe_ostream : public std::basic_ostream<CharT, Traits>
{
private:
    using parent_type = std::basic_ostream<CharT, Traits>;

public:
    using char_type    = CharT;
    using traits_type  = Traits;
    using int_type     = typename Traits::int_type;
    using pos_type     = typename Traits::pos_type;
    using off_type     = typename Traits::off_type;

public:
    basic_pipe_ostream() = default;

    explicit basic_pipe_ostream(const std::string& name, std::ios_base::openmode mode = std::ios_base::out)
    :parent_type{nullptr}
    {
        parent_type::rdbuf(m_buffer.get());
        open(name, mode);
    }

    virtual ~basic_pipe_ostream() = default;

    basic_pipe_ostream(const basic_pipe_ostream&) = delete;
    basic_pipe_ostream& operator=(const basic_pipe_ostream&) = delete;

    basic_pipe_ostream(basic_pipe_ostream&& other) noexcept
    :parent_type{std::move(other)}
    {
        std::swap(m_buffer, other.m_buffer);
        parent_type::rdbuf(m_buffer.get());
    }

    basic_pipe_ostream& operator=(basic_pipe_ostream&& other) noexcept
    {
        parent_type::operator=(std::move(other));
        std::swap(m_buffer, other.m_buffer);

        parent_type::rdbuf(m_buffer.get());

        return *this;
    }

    void open(const std::string& name, std::ios_base::openmode mode = std::ios_base::out)
    {
        m_buffer->open(name, mode);
        parent_type::clear(m_buffer->is_open() ? std::ios_base::goodbit : std::ios_base::failbit);
    }

    bool is_open() const noexcept
    {
        return m_buffer->is_open();
    }

    void close()
    {
        m_buffer->close();
    }

    basic_pipe_streambuf<char_type, traits_type>* rdbuf() const noexcept
    {
        return m_buffer.get();
    }

private:
    friend class process;
    friend std::pair<basic_pipe_istream<char_type, traits_type>, basic_pipe_ostream<char_type, traits_type>> make_anonymous_pipe<char_type, traits_type>();

    basic_pipe_ostream(basic_pipe_streambuf<char_type, traits_type> buffer)
    :parent_type{nullptr}
    ,m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>(std::move(buffer))}
    {
        parent_type::rdbuf(m_buffer.get());
    }

private:
    std::unique_ptr<basic_pipe_streambuf<char_type, traits_type>> m_buffer{std::make_unique<basic_pipe_streambuf<char_type, traits_type>>()};
};

template<typename CharT, typename Traits>
std::pair<basic_pipe_istream<CharT, Traits>, basic_pipe_ostream<CharT, Traits>> make_anonymous_pipe()
{
    int fd[2];

    if(pipe(fd))
        throw std::runtime_error{"Failed to create pipe"};

    return std::make_pair(basic_pipe_istream<CharT, Traits>{basic_pipe_streambuf<CharT, Traits>{fd[0], std::ios_base::in}},
                          basic_pipe_ostream<CharT, Traits>{basic_pipe_streambuf<CharT, Traits>{fd[1], std::ios_base::out}});
}

using pipe_streambuf = basic_pipe_streambuf<char>;
using pipe_istream = basic_pipe_istream<char>;
using pipe_ostream = basic_pipe_ostream<char>;

}

#endif

#endif
