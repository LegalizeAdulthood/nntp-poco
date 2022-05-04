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


void NNTPClientSession::login(const std::string& hostname, std::string& response)
{
	open();
	int status = sendCommand("EHLO", hostname, response);
	if (isPermanentNegative(status))
		status = sendCommand("HELO", hostname, response);
	if (!isPositiveCompletion(status)) throw NNTPException("Login failed", response, status);
}


void NNTPClientSession::login(const std::string& hostname)
{
	std::string response;
	login(hostname, response);
}


void NNTPClientSession::login()
{
	login(Environment::nodeName());
}


void NNTPClientSession::loginUsingLogin(const std::string& username, const std::string& password)
{
	std::string response;
	int status = sendCommand("AUTH LOGIN", response);
	if (!isPositiveIntermediate(status)) throw NNTPException("Cannot authenticate using LOGIN", response, status);

	std::ostringstream usernameBase64;
	Base64Encoder usernameEncoder(usernameBase64);
	usernameEncoder.rdbuf()->setLineLength(0);
	usernameEncoder << username;
	usernameEncoder.close();

	std::ostringstream passwordBase64;
	Base64Encoder passwordEncoder(passwordBase64);
	passwordEncoder.rdbuf()->setLineLength(0);
	passwordEncoder << password;
	passwordEncoder.close();

	//Server request for username/password not defined could be either
	//S: login:
	//C: user_login
	//S: password:
	//C: user_password
	//or
	//S: password:
	//C: user_password
	//S: login:
	//C: user_login

	std::string decodedResponse;
	std::istringstream responseStream(response.substr(4));
	Base64Decoder responseDecoder(responseStream);
	StreamCopier::copyToString(responseDecoder, decodedResponse);

	if (Poco::icompare(decodedResponse, 0, 8, "username") == 0) // username first (md5("Username:"))
	{
		status = sendCommand(usernameBase64.str(), response);
		if (!isPositiveIntermediate(status)) throw NNTPException("Login using LOGIN username failed", response, status);

		status = sendCommand(passwordBase64.str(), response);
		if (!isPositiveCompletion(status)) throw NNTPException("Login using LOGIN password failed", response, status);
	}
	else if  (Poco::icompare(decodedResponse, 0, 8, "password") == 0) // password first (md5("Password:"))
	{
		status = sendCommand(passwordBase64.str(), response);
		if (!isPositiveIntermediate(status)) throw NNTPException("Login using LOGIN password failed", response, status);

		status = sendCommand(usernameBase64.str(), response);
		if (!isPositiveCompletion(status)) throw NNTPException("Login using LOGIN username failed", response, status);
	}
}


void NNTPClientSession::loginUsingPlain(const std::string& username, const std::string& password)
{
	std::ostringstream credentialsBase64;
	Base64Encoder credentialsEncoder(credentialsBase64);
	credentialsEncoder.rdbuf()->setLineLength(0);
	credentialsEncoder << '\0' << username << '\0' << password;
	credentialsEncoder.close();

	std::string response;
	int status = sendCommand("AUTH PLAIN", credentialsBase64.str(), response);
	if (!isPositiveCompletion(status)) throw NNTPException("Login using PLAIN failed", response, status);
}


void NNTPClientSession::login(LoginMethod loginMethod, const std::string& username, const std::string& password)
{
	login(Environment::nodeName(), loginMethod, username, password);
}


void NNTPClientSession::login(const std::string& hostname, LoginMethod loginMethod, const std::string& username, const std::string& password)
{
	std::string response;
	login(hostname, response);

	if (loginMethod == AUTH_LOGIN)
	{
		if (response.find("LOGIN", 0) != std::string::npos)
		{
			loginUsingLogin(username, password);
		}
		else throw NNTPException("The news service does not support LOGIN authentication", response);
	}
	else if (loginMethod == AUTH_PLAIN)
	{
		if (response.find("PLAIN", 0) != std::string::npos)
		{
			loginUsingPlain(username, password);
		}
		else throw NNTPException("The news service does not support PLAIN authentication", response);
	}
	else if (loginMethod != AUTH_NONE)
	{
		throw NNTPException("The autentication method is not supported");
	}
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


void NNTPClientSession::sendCommands(const MailMessage& message, const Recipients* pRecipients)
{
	std::string response;
	int status = 0;
	const std::string& fromField = message.getSender();
	std::string::size_type emailPos = fromField.find('<');
	if (emailPos == std::string::npos)
	{
		std::string sender("<");
		sender.append(fromField);
		sender.append(">");
		status = sendCommand("MAIL FROM:", sender, response);
	}
	else
	{
		status = sendCommand("MAIL FROM:", fromField.substr(emailPos, fromField.size() - emailPos), response);
	}

	if (!isPositiveCompletion(status)) throw NNTPException("Cannot send message", response, status);

	std::ostringstream recipient;
	if (pRecipients)
	{
		if (pRecipients->empty()) throw Poco::InvalidArgumentException("attempting to send message with empty recipients list");
		for (const auto& rec: *pRecipients)
		{
			recipient << '<' << rec << '>';
			int status = sendCommand("RCPT TO:", recipient.str(), response);
			if (!isPositiveCompletion(status)) throw NNTPException(std::string("Recipient rejected: ") + recipient.str(), response, status);
			recipient.str("");
		}
	}
	else
	{
		if (message.recipients().empty()) throw Poco::InvalidArgumentException("attempting to send message with empty recipients list");
		for (const auto& rec: message.recipients())
		{
			recipient << '<' << rec.getAddress() << '>';
			int status = sendCommand("RCPT TO:", recipient.str(), response);
			if (!isPositiveCompletion(status)) throw NNTPException(std::string("Recipient rejected: ") + recipient.str(), response, status);
			recipient.str("");
		}
	}

	status = sendCommand("DATA", response);
	if (!isPositiveIntermediate(status)) throw NNTPException("Cannot send message data", response, status);
}


void NNTPClientSession::sendAddresses(const std::string& from, const Recipients& recipients)
{
	std::string response;
	int status = 0;

	std::string::size_type emailPos = from.find('<');
	if (emailPos == std::string::npos)
	{
		std::string sender("<");
		sender.append(from);
		sender.append(">");
		status = sendCommand("MAIL FROM:", sender, response);
	}
	else
	{
		status = sendCommand("MAIL FROM:", from.substr(emailPos, from.size() - emailPos), response);
	}

	if (!isPositiveCompletion(status)) throw NNTPException("Cannot send message", response, status);

	std::ostringstream recipient;

	if (recipients.empty()) throw Poco::InvalidArgumentException("attempting to send message with empty recipients list");
	for (const auto& rec: recipients)
	{
		recipient << '<' << rec << '>';
		int status = sendCommand("RCPT TO:", recipient.str(), response);
		if (!isPositiveCompletion(status)) throw NNTPException(std::string("Recipient rejected: ") + recipient.str(), response, status);
		recipient.str("");
	}
}


void NNTPClientSession::sendData()
{
	std::string response;
	int status = sendCommand("DATA", response);
	if (!isPositiveIntermediate(status)) throw NNTPException("Cannot send message data", response, status);
}

std::vector<std::string> NNTPClientSession::capabilities()
{
    std::string response;
    int status = sendCommand("CAPABILITIES", response);
	if (!isPositiveInformation(status)) throw NNTPException("Cannot get capabilities", response, status);

    std::vector<std::string> capabilities;
    std::string line;
    _socket.receiveMessage(line);
    while (line != ".")
    {
        capabilities.push_back(line);
        _socket.receiveMessage(line);
    }

    return capabilities;
}

std::vector<std::string> NNTPClientSession::listNewsGroups(const std::string &wildMat)
{
    std::string response;
    int status = sendCommand("LIST NEWSGROUPS", wildMat, response);
    if (!isPositiveCompletion(status)) throw NNTPException("Cannot list newsgroups", response, status);

    std::vector<std::string> newsgroups;
    std::string line;
    _socket.receiveMessage(line);
    while (line != ".")
    {
        newsgroups.push_back(line);
        _socket.receiveMessage(line);
    }

    return newsgroups;
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

    std::vector<std::string> header;
    std::string line;
    _socket.receiveMessage(line);
    while (line != ".")
    {
        header.push_back(line);
        _socket.receiveMessage(line);
    }

    return header;
}


void NNTPClientSession::sendMessage(const MailMessage& message)
{
	sendCommands(message);
	transportMessage(message);
}


void NNTPClientSession::sendMessage(const MailMessage& message, const Recipients& recipients)
{
	sendCommands(message, &recipients);
	transportMessage(message);
}


void NNTPClientSession::transportMessage(const MailMessage& message)
{
	SocketOutputStream socketStream(_socket);
	MailOutputStream mailStream(socketStream);
	message.write(mailStream);
	mailStream.close();
	socketStream.flush();

	std::string response;
	int status = _socket.receiveStatusMessage(response);
	if (!isPositiveCompletion(status)) throw NNTPException("The server rejected the message", response, status);
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


void NNTPClientSession::sendMessage(std::istream& istr)
{
	std::string response;
	int status = 0;

	SocketOutputStream socketStream(_socket);
	MailOutputStream mailStream(socketStream);
	StreamCopier::copyStream(istr, mailStream);
	mailStream.close();
	socketStream.flush();
	status = _socket.receiveStatusMessage(response);
	if (!isPositiveCompletion(status)) throw NNTPException("The server rejected the message", response, status);
}

POCO_IMPLEMENT_EXCEPTION(NNTPException, NetException, "NNTP Exception")

} } // namespace Poco::Net
