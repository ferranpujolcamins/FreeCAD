/***************************************************************************
 *   (c) Fredrik Johansson (mumme74@github.com) 2019                       *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License (LGPL)   *
 *   as published by the Free Software Foundation; either version 2 of     *
 *   the License, or (at your option) any later version.                   *
 *   for detail see the LICENCE text file.                                 *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with FreeCAD; if not, write to the Free Software        *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 *                                                                         *
 ***************************************************************************/
#include <FCConfig.h>
#include <QApplication>
#include <App/Application.h>
#include <Gui/Application.h>
#include <Gui/GuiApplication.h>
#include <QTextCodec>

#include "MainWindow.h"


#if defined (FC_OS_LINUX) || defined(FC_OS_BSD)
QString myDecoderFunc(const QByteArray &localFileName)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    return codec->toUnicode(localFileName);
}

QByteArray myEncoderFunc(const QString &fileName)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    return codec->fromUnicode(fileName);
}
#endif

int main(int argc, char *argv[])
{
#if defined (FC_OS_LINUX) || defined(FC_OS_BSD)
    // Make sure to setup the Qt locale system before setting LANG and LC_ALL to C.
    // which is needed to use the system locale settings.
    (void)QLocale::system();
#if QT_VERSION < 0x050000
    // http://www.freecadweb.org/tracker/view.php?id=399
    // Because of setting LANG=C the Qt automagic to use the correct encoding
    // for file names is broken. This is a workaround to force the use of UTF-8 encoding
    QFile::setEncodingFunction(myEncoderFunc);
    QFile::setDecodingFunction(myDecoderFunc);
#endif
    // See https://forum.freecadweb.org/viewtopic.php?f=18&t=20600
    // See Gui::Application::runApplication()
    putenv("LC_NUMERIC=C");
    putenv("PYTHONPATH=");
#elif defined(FC_OS_MACOSX)
    (void)QLocale::system();
    putenv("PYTHONPATH=");
#else
    _putenv("PYTHONPATH=");
    // https://forum.freecadweb.org/viewtopic.php?f=4&t=18288
    // https://forum.freecadweb.org/viewtopic.php?f=3&t=20515
    const char* fc_py_home = getenv("FC_PYTHONHOME");
    if (fc_py_home)
        _putenv_s("PYTHONHOME", fc_py_home);
    else
        _putenv("PYTHONHOME=");
#endif

#if defined (FC_OS_WIN32)
    int argc_ = argc;
    QVector<QByteArray> data;
    QVector<char *> argv_;

    // get the command line arguments as unicode string
    {
        QCoreApplication app(argc, argv);
        QStringList args = app.arguments();
        for (QStringList::iterator it = args.begin(); it != args.end(); ++it) {
            data.push_back(it->toUtf8());
            argv_.push_back(data.back().data());
        }
        argv_.push_back(0); // 0-terminated string
    }
#endif

#if PY_MAJOR_VERSION >= 3
#if defined(_MSC_VER) && _MSC_VER <= 1800
    // See InterpreterSingleton::init
    Redirection out(stdout), err(stderr), inp(stdin);
#endif
#endif // PY_MAJOR_VERSION

    // Name and Version of the Application
    App::Application::Config()["ExeName"] = "FreeCAD_IDE";
    App::Application::Config()["ExeVendor"] = "FreeCAD";
    App::Application::Config()["AppDataSkipVendor"] = "true";
    App::Application::Config()["MaintainerUrl"] = "http://www.freecadweb.org/wiki/Main_Page";

    // set the banner (for logging and console)
    //App::Application::Config()["CopyrightInfo"] = sBanner;
    App::Application::Config()["AppIcon"] = "freecad";
    //App::Application::Config()["SplashScreen"] = "freecadsplash";
    //App::Application::Config()["StartWorkbench"] = "StartWorkbench";
    //App::Application::Config()["HiddenDockWindow"] = "Property editor";
    //App::Application::Config()["SplashAlignment" ] = "Bottom|Left";
    //App::Application::Config()["SplashTextColor" ] = "#ffffff"; // white
    //App::Application::Config()["SplashInfoColor" ] = "#c8c8c8"; // light grey

#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
    QGuiApplication::setDesktopFileName(QStringLiteral("org.freecadweb.FreeCAD.desktop"));
#endif

   // try {
        // Init phase ===========================================================
        // sets the default run mode for FC, starts with gui if not overridden in InitConfig...
        App::Application::Config()["RunMode"] = "Gui";
        App::Application::Config()["Console"] = "0";
        App::Application::Config()["LoggingConsole"] = "1";

        // Inits the Application
#if defined (FC_OS_WIN32)
        App::Application::init(argc_, argv_.data());
#else
        App::Application::init(argc, argv);
#endif

    int res;
    //Gui::Application::initApplication();
    { // scope guard to destruct before App::Destruct
        Gui::GUIApplication mainApp(argc, App::Application::GetARGV());

        Gui::Application GuiApp(true);

        Ide::MainWindow mWindow;
        mWindow.show();

        res = mainApp.exec();
    }

    // cleans up
    App::Application::destruct();

    return res;
}
