//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#ifndef HUGGLENUKE_H
#define HUGGLENUKE_H

#include "definitions.hpp"
// now we need to ensure that python is included first, because it simply suck :P
#ifdef PYTHONENGINE
#include <Python.h>
#endif

#include <QDialog>

namespace Ui
{
    class HuggleNuke;
}

namespace Huggle
{
    //! Window that allows user to mass delete pages made by certain users
    class HuggleNuke : public QDialog
    {
            Q_OBJECT
        public:
            explicit HuggleNuke(QWidget *parent = 0);
            ~HuggleNuke();

        private:
            Ui::HuggleNuke *ui;
    };
}
#endif // HUGGLENUKE_H
