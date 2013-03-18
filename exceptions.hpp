#ifndef _EXCEPTIONS_HPP
#define _EXCEPTIONS_HPP
#include <string>
#include <exception>

class GenericException : public std::exception {
    public:
	GenericException() : reason_("") {}
	GenericException(std::string reason) : reason_(reason) {}
	~GenericException() throw() {}
	virtual const char* what() const throw() { return reason_.c_str(); }
    private:
	std::string reason_;
};

class BadData : public GenericException {
    public:
	BadData() : GenericException("Bad Data") {}
};

#endif
