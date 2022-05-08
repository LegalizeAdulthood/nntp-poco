#include <iostream>

#include "NNTPClientSession.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/NumberParser.h"

namespace {

class NewsReader
{
public:
    NewsReader(const std::string &server)
        : m_session(server)
    {
        m_session.open();
        m_groups = m_session.listNewsGroups("gmane.comp.*.boost.*");
    }

    bool selectGroup();
    bool selectArticle() { return true; }

private:
    Poco::Net::NNTPClientSession m_session;
    std::vector<std::string> m_groups;
    std::string m_currentGroup;
};

bool NewsReader::selectGroup()
{
    int group;
    do
    {
        int i = 1;
        for (const std::string &newsgroup : m_groups)
        {
            std::cout << i << ' ' << newsgroup << '\n';
            ++i;
        }
        std::cout << "q Quit\n";
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd == "q")
            return true;

        group = Poco::NumberParser::parse(cmd);
    }
    while (group < 1 || group > static_cast<int>(m_groups.size()));
    m_currentGroup = m_groups[group - 1];
    m_session.selectNewsGroup(m_currentGroup);

    return false;
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        NewsReader reader("news.gmane.io");

        while (reader.selectGroup())
        {
            while (reader.selectArticle())
            {
            }
        }
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
