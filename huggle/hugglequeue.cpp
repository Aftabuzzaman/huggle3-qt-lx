//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "hugglequeue.hpp"
#include "ui_hugglequeue.h"

using namespace Huggle;

HuggleQueue::HuggleQueue(QWidget *parent) : QDockWidget(parent), ui(new Ui::HuggleQueue)
{
    this->ui->setupUi(this);
    this->CurrentFilter = HuggleQueueFilter::DefaultFilter;
    this->layout = new QVBoxLayout(ui->scrollArea);
    this->layout->setMargin(0);
    this->layout->setSpacing(0);
    this->xx = new QWidget();
    this->setWindowTitle(Localizations::HuggleLocalizations->Localize("main-queue"));
    this->frame = new QFrame();
    this->ui->scrollArea->setWidget(this->xx);
    this->xx->setLayout(this->layout);
    this->layout->addWidget(this->frame);
    this->Filters();
    this->ui->comboBox->setCurrentIndex(0);
}

HuggleQueue::~HuggleQueue()
{
    delete this->layout;
    delete this->CurrentFilter;
    delete this->ui;
}

void HuggleQueue::AddItem(WikiEdit *page)
{
    if (page == NULL)
    {
        throw new Exception("WikiEdit *page must not be NULL", "void HuggleQueue::AddItem(WikiEdit *page)");
    }
    page->RegisterConsumer(HUGGLECONSUMER_QUEUE);
    if (Core::HuggleCore->Main != NULL)
    {
        if (Core::HuggleCore->Main->VandalDock != NULL)
        {
            if (Core::HuggleCore->Main->VandalDock->IsParsed(page))
            {
                // we don't even need to insert this page to queue
                page->UnregisterConsumer(HUGGLECONSUMER_QUEUE);
                return;
            }
            Core::HuggleCore->Main->VandalDock->Rescore(page);
        }
    }
    // in case that we don't want to have this edit in queue, we can ignore this
    if (Configuration::HuggleConfiguration->UserConfig_DeleteEditsAfterRevert)
    {
        // check if there was a revert to this edit which is newer than itself
        int i = 0;
        WikiEdit::Lock_EditList->lock();
        while (i < Huggle::WikiEdit::EditList.count())
        {
            // retrieve the edit
            WikiEdit *edit = Huggle::WikiEdit::EditList.at(i);
            i++;
            // if this is a same edit we can go next
            if (edit == page)
            {
                continue;
            }
            // if this edit is not newer we can continue
            if (edit->RevID < page->RevID)
            {
                continue;
            }
            // if edit is not a revert we can continue
            if (!edit->IsRevert)
            {
                continue;
            }
            // if edit is not made to this page, we can continue
            if (edit->Page->PageName != page->Page->PageName)
            {
                continue;
            }
            // we found it
            Huggle::Syslog::HuggleLogs->DebugLog("Ignoring edit to " + page->Page->PageName + " because it was reverted by someone");
            WikiEdit::Lock_EditList->unlock();
            page->UnregisterConsumer(HUGGLECONSUMER_QUEUE);
            return;
        }
        WikiEdit::Lock_EditList->unlock();
    }

    if (Configuration::HuggleConfiguration->UserConfig_TruncateEdits)
    {
        // if we want to keep only newest edits in queue we can remove all older edits made to this page
        this->DeleteOlder(page);
    }

    // so we need to insert the item somehow
    HuggleQueueItemLabel *label = new HuggleQueueItemLabel(this);
    label->Page = page;
    label->SetName(page->Page->PageName);
    if (page->Score <= MINIMAL_SCORE)
    {
        this->layout->addWidget(label);
    } else
    {
        int id = 0;
        if (Configuration::HuggleConfiguration->QueueNewEditsUp)
        {
            while (GetScore(id) > page->Score && GetScore(id) > MINIMAL_SCORE)
            {
                id++;
            }
        }
        else
        {
            while (GetScore(id) >= page->Score && GetScore(id) > MINIMAL_SCORE)
            {
                id++;
            }
        }
        if (id >= this->layout->count() && this->layout->count() > 0)
        {
            id = this->layout->count() - 1;
        }
        this->layout->insertWidget(id, label);
    }
    this->Items.append(label);
    HuggleQueueItemLabel::Count++;
}

void HuggleQueue::Next()
{
    if (HuggleQueueItemLabel::Count < 1)
    {
        return;
    }
    QLayoutItem *i = this->layout->itemAt(0);
    HuggleQueueItemLabel *label = (HuggleQueueItemLabel*)i->widget();
    label->Process(i);
    this->layout->removeItem(i);
    delete label;
}

WikiEdit *HuggleQueue::GetWikiEditByRevID(int RevID)
{
    int c = 0;
    while (c < this->Items.count())
    {
        HuggleQueueItemLabel *item = this->Items.at(c);
        if (item->Page->RevID == RevID)
        {
            return item->Page;
        }
        c++;
    }
    return NULL;
}

bool HuggleQueue::DeleteByRevID(int RevID)
{
    int c = 0;
    while (c < this->Items.count())
    {
        HuggleQueueItemLabel *item = this->Items.at(c);
        if (item->Page->RevID == RevID)
        {
            if (Core::HuggleCore->Main->CurrentEdit == item->Page)
            {
                // we can't delete item that is being reviewed now
                return false;
            }
            return this->DeleteItem(item);
        }
        c++;
    }
    // we didn't find it
    return false;
}

void HuggleQueue::Sort()
{
    int c = 0;
    while (c < this->layout->count() - 1)
    {
        QLayoutItem *i = this->layout->itemAt(c);
        this->ResortItem(i, c);
        c++;
    }
}

void HuggleQueue::SortItemByEdit(WikiEdit *e)
{
    int c = 0;
    while (c < this->layout->count() - 1)
    {
        QLayoutItem *i = this->layout->itemAt(c);
        HuggleQueueItemLabel *x = (HuggleQueueItemLabel*)i->widget();
        if (x->Page == e)
        {
            this->ResortItem(i, c);
            return;
        }
        c++;
    }
}

void HuggleQueue::ResortItem(QLayoutItem *item, int position)
{
    if (position < 0)
    {
        // we don't know the position so we need to calculate it
        position = 0;
        while (position < this->layout->count() - 1)
        {
            if (item == this->layout->itemAt(position))
            {
                break;
            }
            position++;
        }
        if (position == this->layout->count() - 1)
        {
            Syslog::HuggleLogs->DebugLog("Unable to sort the queue because item wasn't present");
            return;
        }
    }

    // first we get the item
    HuggleQueueItemLabel *q1 = (HuggleQueueItemLabel*)item->widget();
    int Score = q1->Page->Score;
    int x = 0;
    bool sorted = true;
    while (x < position)
    {
        // every item on left side has to be lower than this one
        // get the item by index and compare the size
        QLayoutItem *l2 = this->layout->itemAt(x);
        if (l2 != item)
        {
            HuggleQueueItemLabel *q2 = (HuggleQueueItemLabel*)l2->widget();
            if (q2->Page->Score < Score)
            {
                sorted = false;
                break;
            }
        }
        x++;
    }
    while (x < this->layout->count() - 1)
    {
        // every item on right side has to be bigger than this one
        // get the item by index and compare the size
        QLayoutItem *l2 = this->layout->itemAt(x);
        if (l2 != item)
        {
            HuggleQueueItemLabel *q2 = (HuggleQueueItemLabel*)l2->widget();
            if (q2->Page->Score > Score)
            {
                sorted = false;
                break;
            }
        }
        x++;
    }
    if (!sorted)
    {
        // now we need to remove the item and place it again
        // because QT doesn't allow manual insertion of item for unknown reasons, we need to readd whole item
        WikiEdit *page = q1->Page;
        page->RegisterConsumer("HuggleQueue::ResortItem");
        this->DeleteItem(q1);
        this->AddItem(page);
        page->UnregisterConsumer("HuggleQueue::ResortItem");
    }
}

void HuggleQueue::Delete(HuggleQueueItemLabel *item, QLayoutItem *qi)
{
    if (item == NULL)
    {
        throw new Exception("HuggleQueueItemLabel *item must not be NULL in this context",
               "void HuggleQueue::Delete(HuggleQueueItemLabel *item, QLayoutItem *qi)");
    }
    if (qi != NULL)
    {
        item->Page->UnregisterConsumer(HUGGLECONSUMER_QUEUE);
        item->Page = NULL;
        this->layout->removeItem(qi);
        return;
    }
    int curr=0;
    while(curr<(this->layout->count()-1))
    {
        QLayoutItem *i = this->layout->itemAt(curr);
        HuggleQueueItemLabel *label = (HuggleQueueItemLabel*)i->widget();
        if (label == item)
        {
            this->layout->removeItem(i);
            if (label->Page != NULL)
            {
                label->Page->UnregisterConsumer(HUGGLECONSUMER_QUEUE);
                label->Page = NULL;
            }
            break;
        }
        curr++;
    }
}

void HuggleQueue::Trim(int i)
{
    if (i < 1)
    {
        throw new Huggle::Exception("Parameter i must be greater than 0 in HuggleQueue::Trim(i)");
    }

    while (i > 0)
    {
        Trim();
        i--;
    }
}

void HuggleQueue::Trim()
{
    if (HuggleQueueItemLabel::Count < 1)
    {
        return;
    }
    int x = this->layout->count() - 1;
    QLayoutItem *i = this->layout->itemAt(x);
    if (i->widget() == this->frame)
    {
        x--;
        i = this->layout->itemAt(x);
    }
    HuggleQueueItemLabel *label = (HuggleQueueItemLabel*)i->widget();
    label->Remove();
    this->layout->removeItem(i);
    delete label;
}

void HuggleQueue::Filters()
{
    this->ui->comboBox->clear();
    int x = 0;
    int id = 0;
    while (x < HuggleQueueFilter::Filters.count())
    {
        HuggleQueueFilter *FilthyFilter = HuggleQueueFilter::Filters.at(x);
        if (this->CurrentFilter == FilthyFilter)
        {
            id = x;
        }
        x++;
        this->ui->comboBox->addItem(FilthyFilter->QueueName);
    }
    this->ui->comboBox->setCurrentIndex(id);
}

void HuggleQueue::DeleteOlder(WikiEdit *edit)
{
    int i = 0;
    while (i < this->Items.count())
    {
        WikiEdit *_e = this->Items.at(i)->Page;
        if (edit->RevID > _e->RevID)
        {
            if (edit->Page->PageName == _e->Page->PageName)
            {
                Huggle::Syslog::HuggleLogs->DebugLog("Deleting old edit to page " + _e->Page->PageName);
                // remove it
                if (this->DeleteByRevID(_e->RevID))
                {
                    // we can only continue if some edit was deleted
                    // otherwise we end up looping here
                    continue;
                }
            }
        }
        i++;
    }
}

void HuggleQueue::Clear()
{
    // now we need to remove all items
    while (this->Items.count() > 0)
    {
        if (!this->DeleteItem(this->Items.at(0)))
        {
            // we failed to remove the item, break or we end up
            // looping here
            Huggle::Syslog::HuggleLogs->DebugLog("Failed to clear the queue");
            return;
        }
    }
}

long HuggleQueue::GetScore(int id)
{
    if (this->layout->count() - 1 <= id)
    {
        return MINIMAL_SCORE;
    }

    QLayoutItem *i = this->layout->itemAt(id);
    HuggleQueueItemLabel *label = (HuggleQueueItemLabel*)i->widget();
    if (label->Page == NULL)
    {
        return MINIMAL_SCORE;
    }
    return label->Page->Score;
}

bool HuggleQueue::DeleteItem(HuggleQueueItemLabel *item)
{
    HuggleQueueItemLabel::Count--;
    item->close();
    this->Delete(item);
    int removed = this->Items.removeAll(item);
    delete item;
    if (removed > 0)
    {
        return true;
    }
    return false;
}

void HuggleQueue::on_comboBox_currentIndexChanged(int index)
{
    if (index > -1 && index < HuggleQueueFilter::Filters.count())
    {
        this->CurrentFilter = HuggleQueueFilter::Filters.at(index);
    }
}

