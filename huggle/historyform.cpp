//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "historyform.hpp"
#include "ui_historyform.h"

using namespace Huggle;

HistoryForm::HistoryForm(QWidget *parent) : QDockWidget(parent), ui(new Ui::HistoryForm)
{
    this->RetrievedEdit = NULL;
    this->RetrievingEdit = false;
    this->ui->setupUi(this);
    this->ui->pushButton->setEnabled(false);
    this->ui->pushButton->setText(Localizations::HuggleLocalizations->Localize("historyform-no-info"));
    this->ui->tableWidget->setColumnCount(5);
    QStringList header;
    header << Huggle::Localizations::HuggleLocalizations->Localize("user") <<
              Huggle::Localizations::HuggleLocalizations->Localize("size") <<
              Huggle::Localizations::HuggleLocalizations->Localize("summary") <<
              Huggle::Localizations::HuggleLocalizations->Localize("id") <<
              Huggle::Localizations::HuggleLocalizations->Localize("date");
    this->ui->tableWidget->setHorizontalHeaderLabels(header);
    this->ui->tableWidget->verticalHeader()->setVisible(false);
    this->ui->tableWidget->horizontalHeader()->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
#if QT_VERSION >= 0x050000
// Qt5 code
    this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
// Qt4 code
    this->ui->tableWidget->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif
    this->ui->tableWidget->setShowGrid(false);
    this->query = NULL;
    this->t1 = NULL;
}

HistoryForm::~HistoryForm()
{
    if (this->RetrievedEdit != NULL)
    {
        this->RetrievedEdit->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
        this->RetrievedEdit = NULL;
    }
    if (this->query != NULL)
    {
        this->query->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
        this->query = NULL;
    }
    delete this->t1;
    delete this->ui;
}

void HistoryForm::Read()
{
    //this->ui->pushButton->setText(Localizations::HuggleLocalizations->Localize("historyform-retrieving-history"));
    this->ui->pushButton->hide();
    this->query = new ApiQuery();
    this->query->SetAction(ActionQuery);
    this->query->Parameters = "prop=revisions&rvprop=" + QUrl::toPercentEncoding("ids|flags|timestamp|user|userid|size|sha1|comment") + "&rvlimit=" +
            QString::number(Huggle::Configuration::HuggleConfiguration->UserConfig_HistoryMax) +
            "&titles=" + QUrl::toPercentEncoding(this->CurrentEdit->Page->PageName);
    this->query->RegisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
    this->query->Process();
    if (this->t1 != NULL)
    {
        delete this->t1;
    }
    this->t1 = new QTimer(this);
    this->Clear();
    connect(t1, SIGNAL(timeout()), this, SLOT(onTick01()));
    this->t1->start(200);
}

void HistoryForm::Update(WikiEdit *edit)
{
    if (edit == NULL)
    {
        throw new Exception("WikiEdit edit must not be NULL", "void HistoryForm::Update(WikiEdit *edit)");
    }
    this->CurrentEdit = edit;
    this->ui->pushButton->setText(Localizations::HuggleLocalizations->Localize("historyform-retrieve-history"));
    this->ui->pushButton->show();
    this->ui->pushButton->setEnabled(true);
    this->ui->tableWidget->clearContents();
    this->Clear();
    if (this->RetrievedEdit != NULL)
    {
        this->RetrievedEdit->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
        this->RetrievedEdit = NULL;
    }
    if (this->t1 != NULL)
    {
        this->t1->stop();
        delete this->t1;
        this->t1 = NULL;
    }
    if (this->query != NULL)
    {
        this->query->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
        this->query = NULL;
    }
}

void HistoryForm::onTick01()
{
    if (this->RetrievingEdit && this->RetrievedEdit != NULL)
    {
        if (this->RetrievedEdit->IsPostProcessed())
        {
            Core::HuggleCore->Main->ProcessEdit(this->RetrievedEdit, false, true);
            this->RetrievingEdit = false;
            this->RetrievedEdit->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
            this->RetrievedEdit = NULL;
            this->t1->stop();
        }
        return;
    }
    if (this->query == NULL)
    {
        return;
    }
    if (!this->query->Processed())
    {
        return;
    }
    if (this->query->Result->Failed)
    {
        this->ui->pushButton->setEnabled(true);
        Huggle::Syslog::HuggleLogs->ErrorLog("Unable to retrieve history");
        this->query->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
        this->query = NULL;
        this->t1->stop();
        return;
    }
    QDomDocument d;
    d.setContent(this->query->Result->Data);
    QDomNodeList l = d.elementsByTagName("rev");
    int x=0;
    while (x < l.count())
    {
        QDomElement e = l.at(x).toElement();
        QString RevID;
        if (e.attributes().contains("revid"))
        {
            RevID = e.attribute("revid");
        } else
        {
            x++;
            continue;
        }
        QString user = "<Unknown>";
        if (e.attributes().contains("user"))
        {
            user = e.attribute("user");
        }
        QString size = "<Unknown>";
        if (e.attributes().contains("size"))
        {
            size = e.attribute("size");
        }
        QString date = "<Unknown>";
        if (e.attributes().contains("timestamp"))
        {
            date = e.attribute("timestamp");
        }
        QString summary = "No summary";
        if (e.attributes().contains("comment"))
        {
            if (e.attribute("comment") != "")
            {
                summary = e.attribute("comment");
            }
        }
        this->ui->tableWidget->insertRow(x);
        if (this->CurrentEdit->RevID == RevID.toInt())
        {
            QFont font;
            font.setBold(true);
            QTableWidgetItem *i = new QTableWidgetItem(user);
            i->setFont(font);
            this->ui->tableWidget->setItem(x, 0, i);
            i = new QTableWidgetItem(size);
            i->setFont(font);
            this->ui->tableWidget->setItem(x, 1, i);
            i = new QTableWidgetItem(summary);
            i->setFont(font);
            this->ui->tableWidget->setItem(x, 2, i);
            i = new QTableWidgetItem(RevID);
            i->setFont(font);
            this->ui->tableWidget->setItem(x, 3, i);
            i = new QTableWidgetItem(date);
            i->setFont(font);
            this->ui->tableWidget->setItem(x, 4, i);
        } else
        {
            this->ui->tableWidget->setItem(x, 0, new QTableWidgetItem(user));
            this->ui->tableWidget->setItem(x, 1, new QTableWidgetItem(size));
            this->ui->tableWidget->setItem(x, 2, new QTableWidgetItem(summary));
            this->ui->tableWidget->setItem(x, 3, new QTableWidgetItem(RevID));
            this->ui->tableWidget->setItem(x, 4, new QTableWidgetItem(date));
        }
        x++;
    }
    this->query->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
    this->query = NULL;
    this->t1->stop();
}

void HistoryForm::on_pushButton_clicked()
{
    this->Read();
}

void HistoryForm::on_tableWidget_clicked(const QModelIndex &index)
{
    if (this->query != NULL || this->RetrievingEdit)
    {
        // we must not retrieve edit until previous operation did finish
        return;
    }

    if (this->ui->tableWidget->rowCount() == 0 || this->CurrentEdit == NULL)
    {
        return;
    }

    this->RetrievingEdit = true;
    // check if we don't have this edit in a buffer
    int x = 0;
    int revid = this->ui->tableWidget->item(index.row(), 3)->text().toInt();
    if (revid == 0)
    {
        this->RetrievingEdit = false;
        return;
    }
    WikiEdit::Lock_EditList->lock();
    while (x < WikiEdit::EditList.count())
    {
        WikiEdit *edit = WikiEdit::EditList.at(x);
        x++;
        if (edit->RevID == revid)
        {
            Core::HuggleCore->Main->ProcessEdit(edit, true, true);
            this->RetrievingEdit = false;
            WikiEdit::Lock_EditList->unlock();
            return;
        }
    }
    WikiEdit::Lock_EditList->unlock();
    // there is no such edit, let's get it
    WikiEdit *w = new WikiEdit();
    w->User = new WikiUser(this->ui->tableWidget->item(index.row(), 0)->text());
    w->Page = new WikiPage(this->CurrentEdit->Page);
    w->RevID = revid;
    w->RegisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
    if (this->RetrievedEdit != NULL)
    {
        this->RetrievedEdit->UnregisterConsumer(HUGGLECONSUMER_HISTORYWIDGET);
    }
    Core::HuggleCore->PostProcessEdit(w);
    if (this->t1 != NULL)
    {
        delete this->t1;
    }
    this->RetrievedEdit = w;
    Core::HuggleCore->Main->Browser->RenderHtml(Huggle::Localizations::HuggleLocalizations->Localize("wait"));
    this->t1 = new QTimer();
    connect(this->t1, SIGNAL(timeout()), this, SLOT(onTick01()));
    this->t1->start();
}

void HistoryForm::Clear()
{
    while (this->ui->tableWidget->rowCount() > 0)
    {
        this->ui->tableWidget->removeRow(0);
    }
}
