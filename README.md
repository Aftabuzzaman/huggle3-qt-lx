huggle3-qt-lx
=============

Huggle 3 QT-LX is an anti-vandalism tool for use on Wikipedia and other Wikimedia projects, written in C++ (QT framework).
It is supported for Windows and Linux

[![Build Status](https://travis-ci.org/huggle/huggle3-qt-lx.png?branch=ubuntu)](https://travis-ci.org/huggle/huggle3-qt-lx)

This is a repository that is used to build debian packages

Easy
=====

 * git merge master debian
 * run debian.sh

Help
======

 * First we need to download a zip of this package to /tmp
 * Now we can unzip it
 * Now we need to switch to huggle3-qt-lx and create a tarball of huggle folder
=======
Windows
-------------

* Download QT Creator from http://qt-project.org/downloads
* Download OpenSSL and Visual C++ 2008 Redistributables from http://slproweb.com/products/Win32OpenSSL.html
* Checkout this repository
* Create empty version.txt in folder huggle
* Open huggle.pro in that and hit build

Linux
-------------

tar -zcvf huggle_VERSION.orig.tar.gz huggle

 * Now rename the original huggle folder

mv huggle huggle-VERSION

 * switch to it
 * run debuild -us -uc

