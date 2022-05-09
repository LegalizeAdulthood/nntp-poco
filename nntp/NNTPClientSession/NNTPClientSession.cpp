//
// NNTPClientSession.cpp
//
// Library: Net
// Package: Mail
// Module:  NNTPClientSession
//


#include "NNTPClientSession.h"

#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailStream.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Environment.h"
#include "Poco/NumberParser.h"
#include "Poco/StreamCopier.h"
#include "Poco/Base64Encoder.h"
#include "Poco/Base64Decoder.h"
#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <iterator>


using Poco::StreamCopier;
using Poco::Base64Encoder;
using Poco::Base64Decoder;
using Poco::Environment;


namespace Poco {
namespace Net {


class DialogStreamBuf: public Poco::UnbufferedStreamBuf
{
public:
	DialogStreamBuf(DialogSocket& socket):
		_socket(socket)
	{
	}
	
	~DialogStreamBuf()
	{
	}
		
private:
	int readFromDevice()
	{
		return _socket.get();
	}
	
	DialogSocket& _socket;
};


class DialogIOS: public virtual std::ios
{
public:
	DialogIOS(DialogSocket& socket):
		_buf(socket)
	{
		poco_ios_init(&_buf);
	}
	
	~DialogIOS()
	{
	}
	
	DialogStreamBuf* rdbuf()
	{
		return &_buf;
	}

protected:
	DialogStreamBuf _buf;
};


class DialogInputStream: public DialogIOS, public std::istream
{
public:
	DialogInputStream(DialogSocket& socket):
		DialogIOS(socket),
		std::istream(&_buf)
	{
	}
		
	~DialogInputStream()
	{
	}
};


NNTPClientSession::NNTPClientSession(const StreamSocket& socket):
	m_socket(socket),
	m_isOpen(false)
{
}


NNTPClientSession::NNTPClientSession(const std::string& host, Poco::UInt16 port):
	m_host(host),
	m_socket(SocketAddress(host, port)),
	m_isOpen(false)
{
}


NNTPClientSession::~NNTPClientSession()
{
	try
	{
		close();
	}
	catch (...)
	{
	}
}


void NNTPClientSession::setTimeout(const Poco::Timespan& timeout)
{
	m_socket.setReceiveTimeout(timeout);
}


Poco::Timespan NNTPClientSession::getTimeout() const
{
	return m_socket.getReceiveTimeout();
}


void NNTPClientSession::open()
{
	if (!m_isOpen)
	{
		std::string response;
		int status = m_socket.receiveStatusMessage(response);
		if (!isPositiveCompletion(status)) throw NNTPException("The news service is unavailable", response, status);
		m_isOpen = true;
	}
}


void NNTPClientSession::close()
{
	if (m_isOpen)
	{
		std::string response;
		sendCommand("QUIT", response);
		m_socket.close();
		m_isOpen = false;
	}
}


std::vector<std::string> NNTPClientSession::multiLineResponse()
{
    std::vector<std::string> response;
    std::string line;
    m_socket.receiveMessage(line);
    while (line != ".")
    {
        response.push_back(line);
        m_socket.receiveMessage(line);
    }

    return response;
}

std::vector<std::string> NNTPClientSession::capabilities()
{
    std::string response;
    int status = sendCommand("CAPABILITIES", response);
	if (!isPositiveInformation(status)) throw NNTPException("Cannot get capabilities", response, status);

    return multiLineResponse();
}

std::vector<GroupDesc> NNTPClientSession::listNewsGroups( const std::string& wildMat )
{
    std::string response;
    int status = sendCommand("LIST NEWSGROUPS", wildMat, response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot list newsgroups", response, status);

    std::vector<std::string> groups = multiLineResponse();
    std::vector<GroupDesc> groupDescs;
    std::transform(groups.begin(), groups.end(), std::back_inserter(groupDescs), [](const std::string &line) {
        auto pos = line.find_first_of(" \t");
        if (pos == std::string::npos)
        {
            return GroupDesc{line, ""};
        }
        return GroupDesc{line.substr(0, pos), line.substr(line.find_first_not_of(" \t", pos + 1))};
        });
    return groupDescs;
}

ActiveNewsGroup NNTPClientSession::selectNewsGroup( const std::string& newsgroup )
{
    std::string response;
    int status = sendCommand("GROUP", newsgroup, response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot set newsgroup", response, status);

    // 211 90986 1 91036 gmane.comp.lib.boost.user
    StringTokenizer groupInfo(response, " ");
    m_newsGroup = newsgroup;
    m_numArticles = NumberParser::parseUnsigned(groupInfo[1]);
    m_lowArticle = NumberParser::parseUnsigned(groupInfo[2]);
    m_highArticle = NumberParser::parseUnsigned(groupInfo[3]);
    return { m_newsGroup, m_numArticles, m_lowArticle, m_highArticle };
}

std::vector<std::string> NNTPClientSession::articleHeader()
{
    std::string response;
    int status = sendCommand("HEAD", response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot get article header", response, status);

    return multiLineResponse();
}

std::vector<std::string> NNTPClientSession::articleRaw()
{
    std::string response;
    int status = sendCommand("ARTICLE", response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot get article body", response, status);

    return multiLineResponse();
}

void NNTPClientSession::article(NewsArticle &article)
{
    std::string response;
    int status = sendCommand("ARTICLE", response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot get article body", response, status);

	DialogInputStream sis(m_socket);
	MailInputStream mis(sis);
	article.read(mis);
	while (mis.good()) mis.get(); // read any remaining junk
}

bool NNTPClientSession::stat(uint_t article)
{
    std::string response;
    int status = sendCommand("STAT", std::to_string(article), response);
    return isPositiveCompletion(status);
}

void NNTPClientSession::article(uint_t number, NewsArticle &article)
{
    std::string response;
    int status = sendCommand("ARTICLE", std::to_string(number), response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot get article body", response, status);

    DialogInputStream sis(m_socket);
    MailInputStream mis(sis);
    article.read(mis);
    while (mis.good()) mis.get(); // read any remaining junk
}

int NNTPClientSession::sendCommand(const std::string& command, std::string& response)
{
	m_socket.sendMessage(command);
	return m_socket.receiveStatusMessage(response);
}


int NNTPClientSession::sendCommand(const std::string& command, const std::string& arg, std::string& response)
{
	m_socket.sendMessage(command, arg);
	return m_socket.receiveStatusMessage(response);
}


POCO_IMPLEMENT_EXCEPTION(NNTPException, NetException, "NNTP Exception")

} } // namespace Poco::Net
