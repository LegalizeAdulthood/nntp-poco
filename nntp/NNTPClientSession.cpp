//
// NNTPClientSession.cpp
//
// Library: Net
// Package: Mail
// Module:  NNTPClientSession
//
// Copyright (c) 2005-2008, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "NNTPClientSession.h"

#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/MailStream.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/NetworkInterface.h"
#include "Poco/Net/NTLMCredentials.h"
#include "Poco/Net/SSPINTLMCredentials.h"
#include "Poco/Environment.h"
#include "Poco/HMACEngine.h"
#include "Poco/MD5Engine.h"
#include "Poco/SHA1Engine.h"
#include "Poco/DigestStream.h"
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


using Poco::DigestEngine;
using Poco::HMACEngine;
using Poco::MD5Engine;
using Poco::SHA1Engine;
using Poco::DigestOutputStream;
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
	_socket(socket),
	_isOpen(false)
{
}


NNTPClientSession::NNTPClientSession(const std::string& host, Poco::UInt16 port):
	_host(host),
	_socket(SocketAddress(host, port)),
	_isOpen(false)
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
	_socket.setReceiveTimeout(timeout);
}


Poco::Timespan NNTPClientSession::getTimeout() const
{
	return _socket.getReceiveTimeout();
}


void NNTPClientSession::open()
{
	if (!_isOpen)
	{
		std::string response;
		int status = _socket.receiveStatusMessage(response);
		if (!isPositiveCompletion(status)) throw NNTPException("The news service is unavailable", response, status);
		_isOpen = true;
	}
}


void NNTPClientSession::close()
{
	if (_isOpen)
	{
		std::string response;
		sendCommand("QUIT", response);
		_socket.close();
		_isOpen = false;
	}
}


std::vector<std::string> NNTPClientSession::multiLineResponse()
{
    std::vector<std::string> response;
    std::string line;
    _socket.receiveMessage(line);
    while (line != ".")
    {
        response.push_back(line);
        _socket.receiveMessage(line);
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

std::vector<std::string> NNTPClientSession::listNewsGroups(const std::string &wildMat)
{
    std::string response;
    int status = sendCommand("LIST NEWSGROUPS", wildMat, response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot list newsgroups", response, status);

    return multiLineResponse();
}

void NNTPClientSession::selectNewsGroup(const std::string &newsgroup)
{
    std::string response;
    int status = sendCommand("GROUP", newsgroup, response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot set newsgroup", response, status);

    // 211 90986 1 91036 gmane.comp.lib.boost.user
    StringTokenizer groupInfo(response, " ");
    _newsgroup = newsgroup;
    _numArticles = NumberParser::parseUnsigned(groupInfo[1]);
    _lowArticle = NumberParser::parseUnsigned(groupInfo[2]);
    _highArticle = NumberParser::parseUnsigned(groupInfo[3]);
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

	DialogInputStream sis(_socket);
	MailInputStream mis(sis);
	article.read(mis);
	while (mis.good()) mis.get(); // read any remaining junk
}


int NNTPClientSession::sendCommand(const std::string& command, std::string& response)
{
	_socket.sendMessage(command);
	return _socket.receiveStatusMessage(response);
}


int NNTPClientSession::sendCommand(const std::string& command, const std::string& arg, std::string& response)
{
	_socket.sendMessage(command, arg);
	return _socket.receiveStatusMessage(response);
}


POCO_IMPLEMENT_EXCEPTION(NNTPException, NetException, "NNTP Exception")

} } // namespace Poco::Net
