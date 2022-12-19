#include "audiosource.h"

#include <QtWidgets/QApplication>

// TODO
// Speech to qDebug DONE!
// Create full sentences, merging strings together
// Populate a TextField directly, editing the contents at runtime
// Show Aleix
// wayland integration
// move TextInferrer::process to thread(?)
// QtQuick UI with volume indication
// Model downloader
// cmake stuff
// Tune

int main(int argv, char **args)
{
    QApplication app(argv, args);
    QCoreApplication::setApplicationName("Audio Source Test");

    InputTest input;
    input.show();

    return QCoreApplication::exec();
}
