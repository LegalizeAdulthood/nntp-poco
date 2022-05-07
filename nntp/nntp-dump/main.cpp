#include <iostream>

#include "NNTPClientSession.h"
#include "Poco/Net/MailMessage.h"

namespace {

void print(const std::vector<std::string> &lines)
{
    for (const std::string &line : lines)
    {
        std::cout << line << '\n';
    }
}

void print(const Poco::Net::NewsArticle &article)
{
    std::cout << "From:    " << article.getSender() << '\n';
    std::cout << "Subject: " << article.getSubject() << '\n';
    std::cout << "Type:    " << article.getContentType() << '\n';
    std::cout << '\n';
    std::cout << article.getContent() << '\n';
}

void separator()
{
    std::cout << std::string(70, '=') << "\n\n";
}

}

int main(int argc, char **argv)
{
    try
    {
        Poco::Net::NNTPClientSession session("news.gmane.io");
        session.open();

        print(session.capabilities());
        separator();

        print(session.listNewsGroups("gmane.comp.*.boost.*"));
        separator();

        session.selectNewsGroup("gmane.comp.lib.boost.user");
        separator();

        print(session.articleHeader());
        separator();

        print(session.articleRaw());
        separator();

        Poco::Net::NewsArticle article;
        session.article(article);
        print(article);
        separator();
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
