//
// NNTPClientSession.h
//
// Library: Net
// Package: Mail
// Module:  NNTPClientSession
//
// Definition of the NNTPClientSession class.
//


#ifndef Net_NNTPClientSession_INCLUDED
#define Net_NNTPClientSession_INCLUDED


#include "Poco/Net/Net.h"
#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Exception.h"
#include "Poco/Timespan.h"

#include <string>
#include <utility>
#include <vector>

namespace Poco {
namespace Net {

#define NNTP_API

class MailMessage;

using NewsArticle = MailMessage;

POCO_DECLARE_EXCEPTION(NNTP_API, NNTPException, NetException)

using uint_t = unsigned int;

struct ActiveNewsGroup
{
    std::string newsGroup;
    uint_t numArticles{};
    uint_t lowArticle{};
    uint_t highArticle{};
};

using GroupDesc = std::pair<std::string, std::string>;

class NNTP_API NNTPClientSession
	/// This class implements an Network News
	/// Transfer Protocol (NNTP, RFC 2821)
	/// client for sending e-mail messages.
{
public:
	using Recipients = std::vector<std::string>;

	enum
	{
		NNTP_PORT = 119
	};

	enum LoginMethod
	{
		AUTH_NONE,
		AUTH_LOGIN,
		AUTH_PLAIN
	};

	explicit NNTPClientSession(const StreamSocket& socket);
		/// Creates the NNTPClientSession using
		/// the given socket, which must be connected
		/// to a NNTP server.

	NNTPClientSession(const std::string& host, Poco::UInt16 port = NNTP_PORT);
		/// Creates the NNTPClientSession using a socket connected
		/// to the given host and port.

	virtual ~NNTPClientSession();
		/// Destroys the NNTPClientSession.

	void setTimeout(const Timespan& timeout);
		/// Sets the timeout for socket read operations.

    Timespan getTimeout() const;
		/// Returns the timeout for socket read operations.

	void open();
		/// Reads the initial response from the NNTP server.
		///
		/// Usually called implicitly through login(), but can
		/// also be called explicitly to implement different forms
		/// of NNTP authentication.
		///
		/// Does nothing if called more than once.

	void close();
		/// Sends a QUIT command and closes the connection to the server.
		///
		/// Throws a NNTPException in case of a NNTP-specific error, or a
		/// NetException in case of a general network communication failure.

    std::vector<std::string> capabilities();
    std::vector<GroupDesc> listNewsGroups( const std::string& wildMat );
    ActiveNewsGroup selectNewsGroup( const std::string& newsgroup );
    std::vector<std::string> articleHeader();
    std::vector<std::string> articleRaw();

    void article(NewsArticle &article);
    bool stat(uint_t article);
    void article(uint_t number, NewsArticle &article);

protected:
	enum StatusClass
	{
        NNTP_POSITIVE_INFORMATION  = 1,
		NNTP_POSITIVE_COMPLETION   = 2,
		NNTP_POSITIVE_INTERMEDIATE = 3,
		NNTP_TRANSIENT_NEGATIVE    = 4,
		NNTP_PERMANENT_NEGATIVE    = 5
	};
	enum
	{
		DEFAULT_TIMEOUT = 30000000 // 30 seconds default timeout for socket operations
	};

	static bool isPositiveCompletion(int status);
	static bool isPositiveIntermediate(int status);
    static bool isPositiveInformation(int status);
	static bool isTransientNegative(int status);
	static bool isPermanentNegative(int status);

	DialogSocket& socket();
	const std::string& host() const;

private:
	int sendCommand(const std::string& command, std::string& response);
		/// Sends the given command verbatim to the server
		/// and waits for a response.
		///
		/// Throws a NNTPException in case of a NNTP-specific error, or a
		/// NetException in case of a general network communication failure.

	int sendCommand(const std::string& command, const std::string& arg, std::string& response);
		/// Sends the given command verbatim to the server
		/// and waits for a response.
		///
		/// Throws a NNTPException in case of a NNTP-specific error, or a
		/// NetException in case of a general network communication failure.

    std::vector<std::string> multiLineResponse();

	std::string  m_host;
	DialogSocket m_socket;
	bool         m_isOpen;

    std::string m_newsGroup;
    using uint_t = unsigned int;
    uint_t m_numArticles{};
    uint_t m_lowArticle{};
    uint_t m_highArticle{};
};


//
// inlines
//
inline bool NNTPClientSession::isPositiveCompletion(int status)
{
	return status/100 == NNTP_POSITIVE_COMPLETION;
}


inline bool NNTPClientSession::isPositiveIntermediate(int status)
{
	return status/100 == NNTP_POSITIVE_INTERMEDIATE;
}

inline bool NNTPClientSession::isPositiveInformation( int status )
{
    return status/100 == NNTP_POSITIVE_INFORMATION;
}


inline bool NNTPClientSession::isTransientNegative(int status)
{
	return status/100 == NNTP_TRANSIENT_NEGATIVE;
}


inline bool NNTPClientSession::isPermanentNegative(int status)
{
	return status/100 == NNTP_PERMANENT_NEGATIVE;
}


inline DialogSocket& NNTPClientSession::socket()
{
	return m_socket;
}


inline const std::string& NNTPClientSession::host() const
{
	return m_host;
}


} } // namespace Poco::Net


#endif // Net_NNTPClientSession_INCLUDED
