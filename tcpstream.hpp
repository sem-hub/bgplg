#ifndef _TCPSTREAM_HPP
#define _TCPSTREAM_HPP
#include <string>
#include <streambuf>
#include <iostream>
#include "socket.hpp"

template <class Socket,
          class charT, class traits = std::char_traits<charT> >
class SocketStreamBuffer : public std::basic_streambuf<charT, traits>
{
    typedef std::basic_streambuf<charT, traits> sbuftype;
    typedef typename sbuftype::int_type         int_type;
    typedef charT                               char_type;

public:

    // the buffer will take ownership of the socket (ie. it will close it
    // in the destructor) if takeowner == true
    explicit SocketStreamBuffer(Socket &sock,
        bool takeowner = false, std::streamsize bufsize = 512)
        : rsocket_(sock), ownsocket_(takeowner),
          inbuf_(NULL), outbuf_(NULL), bufsize_(bufsize),
          remained_(0), ownbuffers_(false)
    {
    }

    ~SocketStreamBuffer()
    {
        try
        {
	    _flush();

            if (ownsocket_ == true)
            {
                rsocket_.close();
            }
        }
        catch (...)
        {
            // we don't want exceptions to fly out of here
            // and there is not much we can do with errors
            // in this context anyway
        }

        if (ownbuffers_)
        {
            delete [] inbuf_;
            delete [] outbuf_;
        }
    }

protected:
    sbuftype * setbuf(char_type *s, std::streamsize n)
    {
        if (this->gptr() == NULL)
        {
            setg(s, s + n, s + n);
            setp(s, s + n);
            inbuf_ = s;
            outbuf_ = s;
            bufsize_ = n;
            ownbuffers_ = false;
        }

        return this;
    }

    void _flush()
    {
        rsocket_.write(outbuf_,
            (this->pptr() - outbuf_) * sizeof(char_type));
    }

    int_type overflow(int_type c = traits::eof())
    {
        // this method is supposed to flush the put area of the buffer
        // to the I/O device

        // if the buffer was not already allocated nor set by user,
        // do it just now
        if (this->pptr() == NULL)
        {
            outbuf_ = new char_type[bufsize_];
            ownbuffers_ = true;
        }
        else
        {
            _flush();
        }

        setp(outbuf_, outbuf_ + bufsize_);

        if (c != traits::eof())
        {
            sputc(traits::to_char_type(c));
        }

        return 0;
    }

    int sync()
    {
        // just flush the put area
        _flush();
        setp(outbuf_, outbuf_ + bufsize_);
        return 0;
    }

    int_type underflow()
    {
        // this method is supposed to read some bytes from the I/O device

        // if the buffer was not already allocated nor set by user,
        // do it just now
        if (this->gptr() == NULL)
        {
            inbuf_ = new char_type[bufsize_];
            ownbuffers_ = true;
        }

        if (remained_ != 0)
        {
            inbuf_[0] = remainedchar_;
        }

        size_t readn = rsocket_.read(static_cast<char*>(inbuf_) + remained_,
            bufsize_ * sizeof(char_type) - remained_);

        // if (readn == 0 && remained_ != 0)
        // error - there is not enough bytes for completing
        // the last character before the end of the stream
        // - this can mean error on the remote end

        if (readn == 0)
        {
            return traits::eof();
        }

        size_t totalbytes = readn + remained_;
        setg(inbuf_, inbuf_,
            inbuf_ + totalbytes / sizeof(char_type));

        remained_ = totalbytes % sizeof(char_type);
        if (remained_ != 0)
        {
            remainedchar_ = inbuf_[totalbytes / sizeof(char_type)];
        }

        return this->sgetc();
    }

private:

    // not for use
    SocketStreamBuffer(const SocketStreamBuffer&);
    void operator=(const SocketStreamBuffer&);

    Socket &rsocket_;
    bool ownsocket_;
    char_type *inbuf_;
    char_type *outbuf_;
    std::streamsize bufsize_;
    size_t remained_;
    char_type remainedchar_;
    bool ownbuffers_;
};

// this class is an ultimate stream associated with a socket
template <class SocketWrapper,
          class charT, class traits = std::char_traits<charT> >
class SocketGenericStream :
    private SocketStreamBuffer<SocketWrapper, charT, traits>,
    public std::basic_iostream<charT, traits>
{
public:

    // this constructor takes 'ownership' of the socket wrapper if btakeowner == true,
    // so that the socket will be closed in the destructor of the
    // TCPStreamBuffer object
    explicit SocketGenericStream(SocketWrapper &sock, bool takeowner = false)
        : SocketStreamBuffer<SocketWrapper, charT, traits>(sock, takeowner),
          std::basic_iostream<charT, traits>(this)
    {
    }

private:
    // not for use
    SocketGenericStream(const SocketGenericStream&);
    void operator=(const SocketGenericStream&);
};

typedef SocketGenericStream<Socket, char> TCPStream;

#endif
