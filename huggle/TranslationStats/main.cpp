#include <QCoreApplication>
#include <iostream>
#include <math.h>
#include "localization.hpp"

using namespace Huggle;

QString Bar(double size, QString color)
{
    QString text = "<span style=\"color:" + color + "; background-color:" + color + ";\">";
    while (size > 0)
    {
        text += "_";
        size--;
    }
    return text + "</span>";
}

void HTML()
{
    std::cout << "<table>\n<tr><th>Language</th><th>Completed</th><th>Percent</th><th></th></tr>" << std::endl;
    Language *english = Localizations::HuggleLocalizations->LocalizationData.at(0);
    QList<QString> keys = english->Messages.keys();
    int language = 1;
    double max = (double)keys.count();
    while (language < Localizations::HuggleLocalizations->LocalizationData.count())
    {
        double Missing = 0;
        double Untranslated = 0;
        Language *l = Localizations::HuggleLocalizations->LocalizationData.at(language);
        int x = 0;
        while (x < keys.count())
        {
            if (!l->Messages.contains(keys.at(x)))
            {
                Missing++;
            } else if (english->Messages[keys.at(x)] == l->Messages[keys.at(x)])
            {
                Untranslated++;
            }
            x++;
        }
        double Completed = keys.count() - (Missing + Untranslated);
        double Percent = (Completed / max) * 100;
        double rp = round(Percent);
        std::cout << QString("<tr><td>" + l->LanguageName + "</td><td>" + QString::number(Completed)
                     + "/" + QString::number(keys.count())
                     + "</td><td>"  + QString::number(Percent) +  "%</td><td>"
                     + Bar(rp, "green") + Bar(100 - rp, "red")
                     + "</td></tr>").toStdString() << std::endl;
        language++;
    }
    std::cout << "</table>" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Localizations::HuggleLocalizations = new Localizations();
    Localizations::HuggleLocalizations->LocalInit("en");
    Localizations::HuggleLocalizations->LocalInit("ar");
    Localizations::HuggleLocalizations->LocalInit("bg");
    Localizations::HuggleLocalizations->LocalInit("bn");
    Localizations::HuggleLocalizations->LocalInit("cz");
    Localizations::HuggleLocalizations->LocalInit("es");
    Localizations::HuggleLocalizations->LocalInit("de");
    Localizations::HuggleLocalizations->LocalInit("fa");
    Localizations::HuggleLocalizations->LocalInit("fr");
    Localizations::HuggleLocalizations->LocalInit("hi");
    Localizations::HuggleLocalizations->LocalInit("it");
    Localizations::HuggleLocalizations->LocalInit("ja");
    Localizations::HuggleLocalizations->LocalInit("ka");
    Localizations::HuggleLocalizations->LocalInit("km");
    Localizations::HuggleLocalizations->LocalInit("kn");
    Localizations::HuggleLocalizations->LocalInit("ko");
    Localizations::HuggleLocalizations->LocalInit("ml");
    Localizations::HuggleLocalizations->LocalInit("mr");
    Localizations::HuggleLocalizations->LocalInit("nl");
    Localizations::HuggleLocalizations->LocalInit("no");
    Localizations::HuggleLocalizations->LocalInit("oc");
    Localizations::HuggleLocalizations->LocalInit("or");
    Localizations::HuggleLocalizations->LocalInit("pt");
    Localizations::HuggleLocalizations->LocalInit("pt-BR");
    Localizations::HuggleLocalizations->LocalInit("ru");
    Localizations::HuggleLocalizations->LocalInit("sv");
    Localizations::HuggleLocalizations->LocalInit("zh");
    HTML();
    return 0;
}
