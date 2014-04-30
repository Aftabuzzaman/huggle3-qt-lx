//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#ifndef SESSIONFORM_H
#define SESSIONFORM_H

#include "definitions.hpp"
#ifdef PYTHONENGINE
#include <Python.h>
#endif

#include <QDialog>

namespace Ui
{
    class SessionForm;
}

namespace Huggle
{
    //! Session info

    //! Display which user, project, what rights and flags your session have
    class SessionForm : public QDialog
    {
            Q_OBJECT

        public:
            explicit SessionForm(QWidget *parent = 0);
            ~SessionForm();

        private slots:
            void on_pushButton_clicked();

        private:
            Ui::SessionForm *ui;
    };
}

#endif // SESSIONFORM_H
