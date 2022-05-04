#include <iostream>

#include "NNTPClientSession.h"

int main(int argc, char **argv)
{
    try
    {
        Poco::Net::NNTPClientSession session("news.gmane.io");
        session.open();

        std::vector<std::string> capabilities = session.capabilities();
        for (const std::string &cap : capabilities)
        {
            std::cout << cap << '\n';
        }
        std::cout << '\n';

        for (const std::string &newsgroup : session.listNewsGroups("gmane.comp.*.boost.*"))
        {
            std::cout << newsgroup << '\n';
        }
        std::cout << '\n';

        session.selectNewsGroup("gmane.comp.lib.boost.user");
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
