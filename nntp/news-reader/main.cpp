#include <iostream>

#include "NNTPClientSession.h"
#include "Poco/Net/MailMessage.h"

namespace {

class NewsReader
{
public:
    NewsReader(const std::string &server)
        : session(server)
    {
        session.open();
    }

private:
    Poco::Net::NNTPClientSession session;
    std::vector<std::string> newsgroups;
};

}

int main(int argc, char **argv)
{
    try
    {
        NewsReader reader("news.gmane.io");
    }
    catch (const Poco::Exception &bang)
    {
        std::cerr << "Poco Exception: " << bang.what() << '\n';
        return 1;
    }
    catch (const std::exception &bang)
    {
        std::cerr << "Exception: " << bang.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
        return 1;
    }

    return 0;
}
