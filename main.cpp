#include "mainwindow.h"
#include <QApplication>
#include <QtGlobal>
#include <stdio.h>
#include <stdlib.h>

FILE *pFile= fopen("appOutput.log", "w");

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    fprintf(pFile, "%s", localMsg.constData());
    fflush(pFile);

}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput); // Install the handler

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();

    fclose (pFile);
}
