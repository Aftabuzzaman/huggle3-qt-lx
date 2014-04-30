//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#ifndef WIKIUTIL_HPP
#define WIKIUTIL_HPP

#include "definitions.hpp"
// now we need to ensure that python is included first, seriously, Python.h is shitty enough that it requires to be
// included first. Don't believe it? See this:
// http://stackoverflow.com/questions/20300201/why-python-h-of-python-3-2-must-be-included-as-first-together-with-qt4
#ifdef PYTHONENGINE
#include <Python.h>
#endif

#include <QString>
#include "editquery.hpp"
#include "message.hpp"
#include "revertquery.hpp"
#include "wikipage.hpp"

namespace Huggle
{
    class EditQuery;
    class RevertQuery;
    class Message;
    class WikiPage;

    namespace WikiUtil
    {
        bool IsRevert(QString Summary);
        //! Return a localized month for a current wiki
        QString MonthText(int n);
        /*!
         * \brief RevertEdit Reverts the edit
         * \param _e Pointer to edit that needs to be reverted
         * \param summary Summary to use if this is empty the default revert summary is used
         * \param minor If revert should be considered as minor edit
         * \param rollback If rollback feature should be used
         * \param keep Whether the query produced by this function should not be automatically deleted
         * \return Pointer to api query that executes this revert
         */
        RevertQuery *RevertEdit(WikiEdit* _e, QString summary = "", bool minor = false, bool rollback = true, bool keep = false);
        /*!
         * \brief MessageUser Message user
         *
         * This function will deliver a message to user using Message class which is returned by this function
         *
         * \param User Pointer to user
         * \param Text Text of message
         * \param Title Title of message
         * \param Summary Summary
         * \param InsertSection Whether this message should be created in a new section
         * \param DependencyRevert Rollback that is used as a dependency, if it's not NULL
         * the system will wait for it to finish before the message is sent
         * \param NoSuffix will not append huggle suffix if this is true, useful if you need to use custom summary
         *
         * \return NULL on error or instance of Huggle::Message in case it's success
         */
        Message *MessageUser(WikiUser *User, QString Text, QString Title, QString Summary, bool InsertSection = true,
                             Query *Dependency = NULL, bool NoSuffix = false, bool SectionKeep = false,
                             bool autoremove = false, QString BaseTimestamp = "", bool CreateOnly_ = false, bool FreshOnly_ = false);
        void FinalizeMessages();
        /*!
         * \brief EditPage Run a new EditQuery that will edit the page
         *
         * NOTE: References are incremented during query creation so that you can work with it later, in case you don't want to work
         *       with returned value you have to decrement its reference count, otherwise it will never be collected out
         * \param page
         * \param text
         * \param summary
         * \param minor
         * \param BaseTimestamp
         * \param section
         * \return New instance of edit query
         */
        EditQuery *EditPage(QString page, QString text, QString summary = "Edited using huggle", bool minor = false,
                            QString BaseTimestamp = "", unsigned int section = 0);
        EditQuery *EditPage(WikiPage *page, QString text, QString summary = "Edited using huggle",
                            bool minor = false, QString BaseTimestamp = "");
    }
}

#endif // WIKIUTIL_HPP
