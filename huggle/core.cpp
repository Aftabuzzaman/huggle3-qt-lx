//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "core.hpp"

using namespace Huggle;

// definitions
Core  *Core::HuggleCore = NULL;

void Core::Init()
{
    this->StartupTime = QDateTime::currentDateTime();
    // preload of config
    Configuration::HuggleConfiguration->WikiDB = Configuration::GetConfigurationPath() + "wikidb.xml";
    if (Configuration::HuggleConfiguration->_SafeMode)
    {
        Syslog::HuggleLogs->Log("DEBUG: Huggle is running in a safe mode");
    }
    this->gc = new GC();
    GC::gc = this->gc;
    Query::NetworkManager = new QNetworkAccessManager();
    Core::VersionRead();
#if QT_VERSION >= 0x050000
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#else
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
#endif
    Syslog::HuggleLogs->Log("Huggle 3 QT-LX, version " + Configuration::HuggleConfiguration->HuggleVersion);
    Resources::Init();
    Syslog::HuggleLogs->Log("Loading configuration");
    this->Processor = new ProcessorThread();
    this->Processor->start();
    this->LoadLocalizations();
    Configuration::LoadConfig();
    Syslog::HuggleLogs->DebugLog("Loading defs");
    this->LoadDefs();
    Syslog::HuggleLogs->DebugLog("Loading wikis");
    this->LoadDB();
    Syslog::HuggleLogs->DebugLog("Loading queue");
    // these are separators that we use to parse words, less we have, faster huggle will be, despite it will fail more to detect vandals
    // keep it low but precise enough
    Configuration::HuggleConfiguration->SystemConfig_WordSeparators << " " << "." << "," << "(" << ")" << ":" << ";" << "!" << "?" << "/" << "<" << ">" << "[" << "]";
    HuggleQueueFilter::Filters.append(HuggleQueueFilter::DefaultFilter);
    if (!Configuration::HuggleConfiguration->_SafeMode)
    {
#ifdef PYTHONENGINE
        Syslog::HuggleLogs->Log("Loading python engine");
        this->Python = new Python::PythonEngine(Configuration::GetExtensionsRootPath());
#endif
        Syslog::HuggleLogs->Log("Loading plugins in " + Configuration::GetExtensionsRootPath());
        this->ExtensionLoad();
    } else
    {
        Syslog::HuggleLogs->Log("Not loading plugins in a safe mode");
    }
    Syslog::HuggleLogs->Log("Loaded in " + QString::number(this->StartupTime.msecsTo(QDateTime::currentDateTime())));
}

Core::Core()
{
#ifdef PYTHONENGINE
    this->Python = NULL;
#endif
    this->Main = NULL;
    this->fLogin = NULL;
    this->SecondaryFeedProvider = NULL;
    this->PrimaryFeedProvider = NULL;
    this->Processor = NULL;
    this->StartupTime = QDateTime::currentDateTime();
    this->Running = true;
    this->gc = NULL;
}

Core::~Core()
{
    delete this->Main;
    delete this->fLogin;
    delete this->SecondaryFeedProvider;
    delete this->PrimaryFeedProvider;
    delete this->gc;
    delete this->Processor;
}

void Core::LoadDB()
{
    Configuration::HuggleConfiguration->ProjectList.clear();
    Configuration::HuggleConfiguration->ProjectList << Configuration::HuggleConfiguration->Project;
    QString text = "";
    if (QFile::exists(Configuration::HuggleConfiguration->WikiDB))
    {
        QFile db(Configuration::HuggleConfiguration->WikiDB);
        if (!db.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            Syslog::HuggleLogs->ErrorLog("Unable to read " + Configuration::HuggleConfiguration->WikiDB);
            return;
        }
        text = QString(db.readAll());
        db.close();
    }

    if (text == "")
    {
        QFile vf(":/huggle/resources/Resources/Definitions.txt");
        vf.open(QIODevice::ReadOnly);
        text = QString(vf.readAll());
        vf.close();
    }

    QDomDocument d;
    d.setContent(text);
    QDomNodeList list = d.elementsByTagName("wiki");
    int xx=0;
    while (xx < list.count())
    {
        QDomElement e = list.at(xx).toElement();
        if (!e.attributes().contains("name"))
        {
            continue;
        }
        if (!e.attributes().contains("url"))
        {
            continue;
        }
        WikiSite *site = new WikiSite(e.attribute("name"), e.attribute("url"));
        site->IRCChannel = "";
        site->SupportOAuth = false;
        site->SupportHttps = false;
        site->WhiteList = "test";
        // name="testwiki" url="test.wikipedia.org/" path="wiki/" script="w/" https="true" oauth="true" channel="#test.wikipedia" wl="test
        if (e.attributes().contains("path"))
        {
            site->LongPath = e.attribute("path");
        }
        if (e.attributes().contains("wl"))
        {
            site->WhiteList = e.attribute("wl");
        }
        if (e.attributes().contains("script"))
        {
            site->ScriptPath = e.attribute("script");
        }
        if (e.attributes().contains("https"))
        {
            site->SupportHttps = Configuration::SafeBool(e.attribute("https"));
        }
        if (e.attributes().contains("oauth"))
        {
            site->SupportOAuth = Configuration::SafeBool(e.attribute("oauth"));
        }
        if (e.attributes().contains("channel"))
        {
            site->IRCChannel = e.attribute("channel");
        }
        Configuration::HuggleConfiguration->ProjectList.append(site);
        xx++;
    }
}

void Core::DeleteEdit(WikiEdit *edit)
{
    if (edit == NULL)
    {
        return;
    }

    if (edit->Previous != NULL && edit->Next != NULL)
    {
        edit->Previous->Next = edit->Next;
        edit->Next->Previous = edit->Previous;
        edit->Previous = NULL;
        edit->Next = NULL;
        edit->UnregisterConsumer(HUGGLECONSUMER_MAINFORM);
        return;
    }

    if (edit->Previous != NULL)
    {
        edit->Previous->Next = NULL;
        edit->Previous = NULL;
    }

    if (edit->Next != NULL)
    {
        edit->Next->Previous = NULL;
        edit->Next = NULL;
    }

    edit->UnregisterConsumer(HUGGLECONSUMER_MAINFORM);
}

void Core::SaveDefs()
{
    QFile file(Configuration::GetConfigurationPath() + "users.xml");
    if (QFile(Configuration::GetConfigurationPath() + "users.xml").exists())
    {
        QFile(Configuration::GetConfigurationPath() + "users.xml").copy(Configuration::GetConfigurationPath() + "users.xml~");
        QFile(Configuration::GetConfigurationPath() + "users.xml").remove();
    }
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
    {
        Huggle::Syslog::HuggleLogs->ErrorLog("Can't open " + Configuration::GetConfigurationPath() + "users.xml");
        return;
    }
    QString xx = "<definitions>\n";
    WikiUser::TrimProblematicUsersList();
    int x = 0;
    WikiUser::ProblematicUserListLock.lock();
    while (x<WikiUser::ProblematicUsers.count())
    {
        xx += "<user name=\"" + WikiUser::ProblematicUsers.at(x)->Username + "\" badness=\"" +
                QString::number(WikiUser::ProblematicUsers.at(x)->GetBadnessScore()) +"\"></user>\n";
        x++;
    }
    WikiUser::ProblematicUserListLock.unlock();
    xx += "</definitions>";
    file.write(xx.toUtf8());
    file.close();
    QFile().remove(Configuration::GetConfigurationPath() + "users.xml~");
}

QString Core::MonthText(int n)
{
    if (n < 0 || n > 12)
    {
        throw new Huggle::Exception("Month must be between 1 and 12");
    }
    n--;
    return Configuration::HuggleConfiguration->Months.at(n);
}

Message *Core::MessageUser(WikiUser *User, QString Text, QString Title, QString Summary, bool InsertSection,
                           Query *DependencyRevert, bool NoSuffix, bool SectionKeep, bool autoremove, QString bt)
{
    if (User == NULL)
    {
        Huggle::Syslog::HuggleLogs->Log("Cowardly refusing to message NULL user");
        return NULL;
    }

    if (Title == "")
    {
        InsertSection = false;
    }

    Message *m = new Message(User, Text, Summary);
    m->Title = Title;
    m->Dependency = DependencyRevert;
    m->Section = InsertSection;
    m->BaseTimestamp = bt;
    m->SectionKeep = SectionKeep;
    m->Suffix = !NoSuffix;
    Core::Messages.append(m);
    m->RegisterConsumer(HUGGLECONSUMER_CORE);
    if (!autoremove)
    {
        m->RegisterConsumer(HUGGLECONSUMER_CORE_MESSAGE);
    }
    m->Send();
    Huggle::Syslog::HuggleLogs->DebugLog("Sending message to user " + User->Username);

    return m;
}

void Core::LoadDefs()
{
    QFile defs(Configuration::GetConfigurationPath() + "users.xml");
    if (QFile(Configuration::GetConfigurationPath() + "users.xml~").exists())
    {
        Huggle::Syslog::HuggleLogs->Log("WARNING: recovering definitions from last session");
        QFile(Configuration::GetConfigurationPath() + "users.xml").remove();
        if (QFile(Configuration::GetConfigurationPath()
                   + "users.xml~").copy(Configuration::GetConfigurationPath()
                   + "users.xml"))
        {
            QFile().remove(Configuration::GetConfigurationPath() + "users.xml~");
        } else
        {
            Huggle::Syslog::HuggleLogs->Log("WARNING: Unable to recover the definitions");
        }
    }
    if (!defs.exists())
    {
        return;
    }
    defs.open(QIODevice::ReadOnly);
    QString Contents(defs.readAll());
    QDomDocument list;
    list.setContent(Contents);
    QDomNodeList l = list.elementsByTagName("user");
    if (l.count() > 0)
    {
        int i=0;
        while (i<l.count())
        {
            WikiUser *user;
            QDomElement e = l.at(i).toElement();
            if (!e.attributes().contains("name"))
            {
                i++;
                continue;
            }
            user = new WikiUser();
            user->Username = e.attribute("name");
            if (e.attributes().contains("badness"))
            {
                user->SetBadnessScore(e.attribute("badness").toInt());
            }
            WikiUser::ProblematicUsers.append(user);
            i++;
        }
    }
    Syslog::HuggleLogs->DebugLog("Loaded " + QString::number(WikiUser::ProblematicUsers.count()) + " records from last session");
    defs.close();
}

void Core::FinalizeMessages()
{
    if (this->Messages.count() < 1)
    {
        return;
    }
    int x=0;
    QList<Message*> list;
    while (x<this->Messages.count())
    {
        if (this->Messages.at(x)->IsFinished())
        {
            list.append(this->Messages.at(x));
        }
        x++;
    }
    x=0;
    while (x<list.count())
    {
        Message *message = list.at(x);
        message->UnregisterConsumer(HUGGLECONSUMER_CORE);
        Core::Messages.removeOne(message);
        x++;
    }
}

QString Core::RetrieveTemplateToWarn(QString type)
{
    int x=0;
    while (x < Configuration::HuggleConfiguration->LocalConfig_WarningTemplates.count())
    {
        if (HuggleParser::GetKeyFromValue(Configuration::HuggleConfiguration->LocalConfig_WarningTemplates.at(x)) == type)
        {
            return HuggleParser::GetValueFromKey(Configuration::HuggleConfiguration->LocalConfig_WarningTemplates.at(x));
        }
        x++;
    }
    return "";
}

EditQuery *Core::EditPage(WikiPage *page, QString text, QString summary, bool minor, QString BaseTimestamp)
{
    if (page == NULL)
    {
        return NULL;
    }
    // retrieve a token
    EditQuery *eq = new EditQuery();
    if (!summary.endsWith(Configuration::HuggleConfiguration->LocalConfig_EditSuffixOfHuggle))
    {
        summary = summary + " " + Configuration::HuggleConfiguration->LocalConfig_EditSuffixOfHuggle;
    }
    eq->RegisterConsumer("Core::EditPage");
    eq->Page = page->PageName;
    eq->BaseTimestamp = BaseTimestamp;
    this->PendingMods.append(eq);
    eq->text = text;
    eq->Summary = summary;
    eq->Minor = minor;
    eq->Process();
    return eq;
}

void Core::AppendQuery(Query *item)
{
    item->RegisterConsumer("core");
    this->RunningQueries.append(item);
}

void Core::ExtensionLoad()
{
    QString path_ = Configuration::GetExtensionsRootPath();
    if (QDir().exists(path_))
    {
        QDir d(path_);
        QStringList extensions = d.entryList();
        int xx = 0;
        while (xx < extensions.count())
        {
            QString name = extensions.at(xx).toLower();
            if (name.endsWith(".so") || name.endsWith(".dll"))
            {
                name = QString(path_) + extensions.at(xx);
                QPluginLoader *extension = new QPluginLoader(name);
                if (extension->load())
                {
                    QObject* root = extension->instance();
                    if (root)
                    {
                        iExtension *interface = qobject_cast<iExtension*>(root);
                        if (!interface)
                        {
                            Huggle::Syslog::HuggleLogs->Log("Unable to cast the library to extension");
                        }else
                        {
                            if (interface->RequestNetwork())
                            {
                                interface->Networking = Query::NetworkManager;
                            }
                            if (interface->RequestConfiguration())
                            {
                                interface->Configuration = Configuration::HuggleConfiguration;
                            }
                            if (interface->RequestCore())
                            {
                                interface->HuggleCore = Core::HuggleCore;
                            }
                            if (interface->Register())
                            {
                                Core::Extensions.append(interface);
                                Huggle::Syslog::HuggleLogs->Log("Successfully loaded: " + extensions.at(xx));
                            }
                            else
                            {
                                Huggle::Syslog::HuggleLogs->Log("Unable to register: " + extensions.at(xx));
                            }
                        }
                    }
                } else
                {
                    Huggle::Syslog::HuggleLogs->Log("Failed to load (reason: " + extension->errorString() + "): " + extensions.at(xx));
                    delete extension;
                }
            } else if (name.endsWith(".py"))
            {
#ifdef PYTHONENGINE
                name = QString(path_) + extensions.at(xx);
                if (Core::Python->LoadScript(name))
                {
                    Huggle::Syslog::HuggleLogs->Log("Loaded python script: " + name);
                } else
                {
                    Huggle::Syslog::HuggleLogs->Log("Failed to load a python script: " + name);
                }
#endif
            }
            xx++;
        }
    } else
    {
        Huggle::Syslog::HuggleLogs->Log("There is no extensions folder, skipping load");
    }
    Huggle::Syslog::HuggleLogs->Log("Extensions: " + QString::number(Core::Extensions.count()));
}

void Core::VersionRead()
{
    QFile *vf = new QFile(":/huggle/git/version.txt");
    vf->open(QIODevice::ReadOnly);
    QString version(vf->readAll());
    version = version.replace("\n", "");
    Configuration::HuggleConfiguration->HuggleVersion += " " + version;
#if PRODUCTION_BUILD
    Configuration::HuggleConfiguration->HuggleVersion += " production";
#endif
    vf->close();
    delete vf;
}

QString Core::GetProjectURL(WikiSite Project)
{
    return Configuration::HuggleConfiguration->GetURLProtocolPrefix() + Project.URL;
}

QString Core::GetProjectWikiURL(WikiSite Project)
{
    return Core::GetProjectURL(Project) + Project.LongPath;
}

QString Core::GetProjectScriptURL(WikiSite Project)
{
    return Core::GetProjectURL(Project) + Project.ScriptPath;
}

QString Core::GetProjectURL()
{
    return Configuration::GetURLProtocolPrefix() + Configuration::HuggleConfiguration->Project->URL;
}

QString Core::GetProjectWikiURL()
{
    return Core::GetProjectURL(Configuration::HuggleConfiguration->Project) + Configuration::HuggleConfiguration->Project->LongPath;
}

QString Core::GetProjectScriptURL()
{
    return Core::GetProjectURL(Configuration::HuggleConfiguration->Project) + Configuration::HuggleConfiguration->Project->ScriptPath;
}

QString Core::ParameterizedTitle(QString title, QString parameter)
{
    title = title.replace("$1", parameter);
    return title;
}

void Core::ProcessEdit(WikiEdit *e)
{
    Core::Main->ProcessEdit(e);
}

void Core::Shutdown()
{
    this->Running = false;
    // grace time for subthreads to finish
    if (this->Main != NULL)
    {
        this->Main->hide();
    }
    Syslog::HuggleLogs->Log("SHUTDOWN: giving a gracetime to other threads to finish");
    Sleeper::msleep(200);
    if (this->Processor->isRunning())
    {
        this->Processor->exit();
    }
    Core::SaveDefs();
    Configuration::SaveConfig();
#ifdef PYTHONENGINE
    if (!Configuration::HuggleConfiguration->_SafeMode)
    {
        Huggle::Syslog::HuggleLogs->Log("Unloading python");
        delete this->Python;
    }
#endif
    delete Configuration::HuggleConfiguration;
    delete Localizations::HuggleLocalizations;
    delete GC::gc;
    GC::gc = NULL;
    this->gc = NULL;
    QApplication::quit();
}

bool Core::IsRevert(QString Summary)
{
    if (Summary != "")
    {
        int xx = 0;
        while (xx < Configuration::HuggleConfiguration->RevertPatterns.count())
        {
            if (Summary.contains(Configuration::HuggleConfiguration->RevertPatterns.at(xx)))
            {
                return true;
            }
            xx++;
        }
    }
    return false;
}

void Core::TestLanguages()
{
    if (Configuration::HuggleConfiguration->SystemConfig_LanguageSanity)
    {
        Language *english = Localizations::HuggleLocalizations->LocalizationData.at(0);
        QList<QString> keys = english->Messages.keys();
        int language = 1;
        while (language < Localizations::HuggleLocalizations->LocalizationData.count())
        {
            Language *l = Localizations::HuggleLocalizations->LocalizationData.at(language);
            int x = 0;
            while (x < keys.count())
            {
                if (!l->Messages.contains(keys.at(x)))
                {
                    Syslog::HuggleLogs->WarningLog("Language " + l->LanguageName + " is missing key " + keys.at(x));
                } else if (english->Messages[keys.at(x)] == l->Messages[keys.at(x)])
                {
                    Syslog::HuggleLogs->WarningLog("Language " + l->LanguageName + " has key " + keys.at(x)
                            + " but its content is identical to english version");
                }
                x++;
            }
            language++;
        }
    }
}

void Core::DeveloperError()
{
    QMessageBox *mb = new QMessageBox();
    mb->setWindowTitle("Function is restricted now");
    mb->setText("You can't perform this action in developer mode, because you aren't logged into the wiki");
    mb->exec();
    delete mb;
}

void Core::PreProcessEdit(WikiEdit *_e)
{
    if (_e == NULL)
    {
        throw new Exception("NULL edit");
    }

    if (_e->Status == StatusProcessed)
    {
        return;
    }

    if (_e->User == NULL)
    {
        throw new Exception("Edit user was NULL in Core::PreProcessEdit");
    }

    if (_e->Bot)
    {
        _e->User->SetBot(true);
    }

    _e->EditMadeByHuggle = _e->Summary.contains(Configuration::HuggleConfiguration->LocalConfig_EditSuffixOfHuggle);

    int x = 0;
    while (x < Configuration::HuggleConfiguration->LocalConfig_Assisted.count())
    {
        if (_e->Summary.contains(Configuration::HuggleConfiguration->LocalConfig_Assisted.at(x)))
        {
            _e->TrustworthEdit = true;
            break;
        }
        x++;
    }

    if (this->IsRevert(_e->Summary))
    {
        _e->IsRevert = true;
        if (this->PrimaryFeedProvider != NULL)
        {
            this->PrimaryFeedProvider->RvCounter++;
        }
        if (Configuration::HuggleConfiguration->UserConfig_DeleteEditsAfterRevert)
        {
            _e->RegisterConsumer("UncheckedReverts");
            this->UncheckedReverts.append(_e);
        }
    }

    _e->Status = StatusProcessed;
}

void Core::PostProcessEdit(WikiEdit *_e)
{
    if (_e == NULL)
    {
        throw new Exception("NULL edit in PostProcessEdit(WikiEdit *_e) is not a valid edit");
    }
    _e->RegisterConsumer(HUGGLECONSUMER_CORE_POSTPROCESS);
    _e->UnregisterConsumer(HUGGLECONSUMER_WIKIEDIT);
    _e->PostProcess();
    Core::ProcessingEdits.append(_e);
}

void Core::CheckQueries()
{
    int curr = 0;
    if (Core::PendingMods.count() > 0)
    {
        while (curr < Core::PendingMods.count())
        {
            if (Core::PendingMods.at(curr)->IsProcessed())
            {
                EditQuery *e = Core::PendingMods.at(curr);
                Core::PendingMods.removeAt(curr);
                e->UnregisterConsumer("Core::EditPage");
            } else
            {
                curr++;
            }
        }
    }
    curr = 0;
    if (Core::RunningQueries.count() < 1)
    {
        return;
    }
    QList<Query*> Finished;
    while (curr < Core::RunningQueries.count())
    {
        Query *q = Core::RunningQueries.at(curr);
        Core::Main->Queries->UpdateQuery(q);
        if (q->IsProcessed())
        {
            Finished.append(q);
            Huggle::Syslog::HuggleLogs->DebugLog("Query finished with: " + q->Result->Data, 6);
            Core::Main->Queries->UpdateQuery(q);
            Core::Main->Queries->RemoveQuery(q);
        }
        curr++;
    }
    curr = 0;
    while (curr < Finished.count())
    {
        Query *item = Finished.at(curr);
        Core::RunningQueries.removeOne(item);
        item->Lock();
        item->UnregisterConsumer("core");
        item->SafeDelete();
        curr++;
    }
}

RevertQuery *Core::RevertEdit(WikiEdit *_e, QString summary, bool minor, bool rollback, bool keep)
{
    if (_e == NULL)
    {
        throw new Exception("NULL edit in RevertEdit(WikiEdit *_e, QString summary, bool minor, bool rollback, bool keep) is not a valid edit");
    }
    if (_e->User == NULL)
    {
        throw new Exception("Object user was NULL in Core::Revert");
    }
    _e->RegisterConsumer("Core::RevertEdit");
    if (_e->Page == NULL)
    {
        throw new Exception("Object page was NULL");
    }

    RevertQuery *query = new RevertQuery(_e);
    if (summary != "")
    {
        query->Summary = summary;
    }
    query->MinorEdit = minor;
    Core::AppendQuery(query);
    if (Configuration::HuggleConfiguration->EnforceManualSoftwareRollback)
    {
        query->UsingSR = true;
    } else
    {
        query->UsingSR = !rollback;
    }
    query->Process();

    if (keep)
    {
        query->RegisterConsumer("keep");
    }

    return query;
}

void Core::ExceptionHandler(Exception *exception)
{
    ExceptionWindow *w = new ExceptionWindow(exception);
    w->exec();
    delete w;
}

void Core::LoadLocalizations()
{
    Localizations::HuggleLocalizations = new Localizations();
    Localizations::HuggleLocalizations->LocalInit("en");
    if (Configuration::HuggleConfiguration->_SafeMode)
    {
        Huggle::Syslog::HuggleLogs->Log("Skipping load of other languages, because of safe mode");
        return;
    }
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
    Localizations::HuggleLocalizations->LocalInit("ptb");
    Localizations::HuggleLocalizations->LocalInit("ru");
    Localizations::HuggleLocalizations->LocalInit("sv");
    Localizations::HuggleLocalizations->LocalInit("zh");
    this->TestLanguages();
}

bool Core::ReportPreFlightCheck()
{
    if (!Configuration::HuggleConfiguration->AskUserBeforeReport)
    {
        return true;
    }
    QMessageBox::StandardButton q = QMessageBox::question(NULL, "Report user"
                  , "This user has already reached warning level 4, so no further templates will be "\
                    "delivered to them. You can report them now, but please, make sure that they already reached the proper "\
                    "number of recent warnings! You can do so by clicking the \"talk page\" button in following form. "\
                    "Keep in mind that this form and this warning is displayed no matter if your revert was successful "\
                    "or not, so you might conflict with other users here (double check if user isn't already reported) "\
                    "Do you want to report this user?"
                  , QMessageBox::Yes|QMessageBox::No);
    if (q == QMessageBox::No)
    {
        return false;
    }
    return true;
}

int Core::RunningQueriesGetCount()
{
    return this->RunningQueries.count();
}

void Core::TruncateReverts()
{
    while (this->UncheckedReverts.count() > 0)
    {
        WikiEdit *edit = this->UncheckedReverts.at(0);
        if (Huggle::Configuration::HuggleConfiguration->UserConfig_DeleteEditsAfterRevert)
        {
            // we need to delete older edits that we know and that is somewhere in queue
            if (this->Main != NULL)
            {
                if (this->Main->Queue1 != NULL)
                {
                    this->Main->Queue1->DeleteOlder(edit);
                }
            }
        }
        this->UncheckedReverts.removeAt(0);
        this->RevertBuffer.append(edit);
    }

    while (this->RevertBuffer.count() > 10)
    {
        WikiEdit *we = this->RevertBuffer.at(0);
        this->RevertBuffer.removeAt(0);
        we->UnregisterConsumer("UncheckedReverts");
    }
}

double Core::GetUptimeInSeconds()
{
    return (double)this->StartupTime.secsTo(QDateTime::currentDateTime());
}

bool HgApplication::notify(QObject *receiver, QEvent *event)
{
    bool done = true;
    try
    {
        done = QApplication::notify(receiver, event);
    }catch (Huggle::Exception *ex)
    {
        Core::ExceptionHandler(ex);
    }catch (Huggle::Exception &ex)
    {
        Core::ExceptionHandler(&ex);
    }
    return done;
}
