#include "NNTPClientSession.h"

#include <Poco/DateTimeFormatter.h>
#include <Poco/Net/MailMessage.h>
#include <Poco/NumberParser.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>

namespace
{

class NewsReader
{
  public:
    explicit NewsReader(const std::string &server) : m_session(server)
    {
        m_session.open();
        m_groupDescs = m_session.listNewsGroups("gmane.comp.*.boost.*");
        std::sort(
            m_groupDescs.begin(), m_groupDescs.end(),
            [](const Poco::Net::GroupDesc &lhs, const Poco::Net::GroupDesc &rhs)
            { return lhs.first < rhs.first; });
    }

    bool selectGroup();
    bool selectArticle();
    void displayArticle();

  private:
    Poco::Net::NNTPClientSession m_session;
    std::vector<Poco::Net::GroupDesc> m_groupDescs;
    std::string m_currentGroup;
    Poco::Net::ActiveNewsGroup m_activeGroup;
    std::map<unsigned int, std::unique_ptr<Poco::Net::NewsArticle>> m_articles;
    unsigned int m_selectedArticle{};
};

bool NewsReader::selectGroup()
{
    int group;
    do
    {
        int i = 1;
        auto pos = std::max_element(
            m_groupDescs.begin(), m_groupDescs.end(),
            [](const Poco::Net::GroupDesc &lhs, const Poco::Net::GroupDesc &rhs)
            { return lhs.first.size() < rhs.first.size(); });
        unsigned int maxLength = pos->first.size() + 1;
        for (const Poco::Net::GroupDesc &groupDesc : m_groupDescs)
        {
            std::cout << std::setw(3) << std::setfill(' ') << i << ' '
                      << groupDesc.first
                      << std::string(maxLength - groupDesc.first.size(), ' ')
                      << groupDesc.second << '\n';
            ++i;
        }
        std::cout << std::setw(3) << std::setfill(' ') << 'q' << " - Quit\n";
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd == "q")
            return false;

        group = Poco::NumberParser::parse(cmd);
    } while (group < 1 || group > static_cast<int>(m_groupDescs.size()));
    m_currentGroup = m_groupDescs[group - 1].first;
    m_activeGroup = m_session.selectNewsGroup(m_currentGroup);
    m_articles.clear();

    return true;
}

bool NewsReader::selectArticle()
{
    for (unsigned int number = m_activeGroup.lowArticle, count = 0;
         number <= m_activeGroup.highArticle && count < 10; ++number)
    {
        if (m_session.stat(number))
        {
            ++count;
            m_articles[number] = std::make_unique<Poco::Net::NewsArticle>();
            m_session.article(number, *m_articles[number]);
        }
    }
    unsigned int number;
    do
    {
        using NumberArticle =
            std::pair<const unsigned int,
                      std::unique_ptr<Poco::Net::NewsArticle>>;
        auto pos = std::max_element(
            m_articles.begin(), m_articles.end(),
            [](const NumberArticle &lhs, const NumberArticle &rhs)
            {
                return std::to_string(lhs.first).size() <
                       std::to_string(rhs.first).size();
            });
        unsigned int maxLength = std::to_string(pos->first).size() + 1;
        for (const auto &article : m_articles)
        {
            std::cout << std::setw(maxLength) << std::setfill(' ')
                      << article.first << ' ' << article.second->getSubject()
                      << '\n';
        }
        std::cout << std::setw(maxLength) << std::setfill(' ') << 'q'
                  << " - Quit\n";
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd == "q")
            return false;

        number = Poco::NumberParser::parseUnsigned(cmd);
    } while (m_articles.find(number) == m_articles.end());

    m_selectedArticle = number;
    return true;
}

void NewsReader::displayArticle()
{
    const Poco::Net::NewsArticle &article = *m_articles[m_selectedArticle];
    std::cout << "        From: " << article.getSender() << '\n'
              << "     Subject: " << article.getSubject() << '\n'
              << "        Date: "
              << Poco::DateTimeFormatter::format(article.getDate(),
                                                 "%w %B %e, %Y")
              << '\n'
              << "Content-Type: " << article.getContentType() << '\n'
              << '\n'
              << article.getContent() << '\n';
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
                reader.displayArticle();
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
