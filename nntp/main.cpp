#include <iostream>

#include "NNTPClientSession.h"

int main(int argc, char **argv)
{
    Poco::Net::NNTPClientSession session("news.gmane.io");
    session.open();

    std::vector<std::string> capabilities = session.capabilities();
    for (const std::string &cap : capabilities)
    {
        std::cout << cap << '\n';
    }

    return 0;
}
