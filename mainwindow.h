/*
Copyright (C) 2019  Anthony Arrowood

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>

#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxchart.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"
using namespace QXlsx;

#include "port.h"
#include "PWCL_game\com.h"


#define i_percentOn    0
#define i_setPoint     1
#define i_fanSpeed     2
#define i_temperature  3
#define i_tempFiltered 4
#define i_time         5
#define i_inputVar     6
#define i_avg_err      7
#define i_score        8


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    void showRequest(const QString & req);
    bool disonnectedPopUpWindow();
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void response(const QString &response);

private slots:

    void on_portComboBox_activated(int index);
    void on_setButton_clicked();

    void on_actionExport_Excel_File_triggered();

private:
    COM com;


    Ui::MainWindow *ui;

    int timerId;
    void timerEvent(QTimerEvent *event);
    PORT port;
    bool validConnection;

    QString csvFileName;
    QString excelFileName;
    QXlsx::Document xldoc;
    QFile csvdoc;
    QMediaPlayer* player;

protected:
    bool event(QEvent *event);

};

#endif // MAINWINDOW_H
