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


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTextCursor>



/*
*   Called when the application is first opened.
*   Configures main window, log files, and more..
*/
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    ui->mainToolBar->close(); // we dont need a toolbar

    // timer is used to repeatedly check for port data until we are connected
    this->timerId = startTimer(250);
    // indicates when we are connected to the port AND the correct arduino program is being run
    this->validConnection = false;
    // have the table resize with the window
    ui->outputTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   // have the table resize with the window
    ui->outputTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // disable any input into the the percent on textbox
    ui->percentOnInput->setEnabled(false);

    /*
    *  Connect functions from the PORT class to functions declared in the MainWIndow class and vice versa.
    */
    connect(&port, &PORT::request, this, &MainWindow::showRequest);     // when the port recieves data it will emit PORT::request thus calling MainWindow::showRequest
    connect(&port, &PORT::disconnected, this, &MainWindow::disonnectedPopUpWindow);
    connect(this, &MainWindow::response, &port, &PORT::L_processResponse);  // whn the set button is clicked, it will emit MainWindow::response thus calling PORT::L_processResponse



    /*
    *  Prepare data logging files. One in excel format named this->excelFileName
    *  and two in csv file format named this->csvFileName, another csv file
    *  with titled with the current date will copy the contents of this->csvFileName.
    */
    // Move to the directy above the executable
    QString execDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(execDir);
    QDir newDir = QDir::current();
    newDir.cdUp();
    QDir::setCurrent(newDir.path());

    // create the media player
    this->player = new QMediaPlayer;
    player->setMedia(QUrl("qrc:/sound/alarm.wav"));

    // Create file titles with the current date and time
    this->excelFileName = "Data-Game.xlsx";

    // give the excel file column headers
    this->xldoc.write( 1 , 1, "Time");
    this->xldoc.write( 1 , 2, "Percent On");
    this->xldoc.write( 1 , 3, "Temperature");
    this->xldoc.write( 1 , 4, "Filtered Temperature");
    this->xldoc.write( 1 , 5, "Set Point");
    this->xldoc.write( 1 , 6, "Fan Speed");


    /*
    * Set up the live plot on the GUI/
    * The set point must have a special scatterstyle so it doesnt connect the lines
    */
    ui->plot->addGraph();
    ui->plot->graph(0)->setName("Set Point");
    ui->plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor("orange"), 8));
    ui->plot->graph(0)->setPen(QPen(Qt::white)); // so we dont see the line connecting the dots
    ui->plot->graph(0)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(1)->setName("Filtered Temperature");
    ui->plot->graph(1)->setPen(QPen(qRgb(0,100,0)));
    ui->plot->graph(1)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(2)->setName("Temperature");
    ui->plot->graph(2)->setPen(QPen(Qt::blue));
    ui->plot->graph(2)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(3)->setName("Percent Heater on");
    ui->plot->graph(3)->setPen(QPen(QColor("purple"))); // line color for the first graph

    ui->plot->xAxis2->setVisible(true);  // show x ticks at top
    ui->plot->xAxis2->setVisible(false); // dont show labels at top
    ui->plot->yAxis2->setVisible(true);  // right y axis labels
    ui->plot->yAxis2->setTickLabels(true);  // show y ticks on right side for temperature

    ui->plot->yAxis->setLabel("Heater [%]");
    ui->plot->yAxis2->setLabel("Temperature [C]");
    ui->plot->xAxis->setLabel("Time [min]");

    // we dont want a legend
    ui->plot->legend->setVisible(false);
    QCPAxisRect *ar = ui->plot->axisRect(0); // get the default range axis and tell it to zoom and drag relative to right side yaxis
    ar->setRangeDragAxes(ui->plot->xAxis, ui->plot->yAxis2);
    ar->setRangeZoomAxes(ui->plot->xAxis, ui->plot->yAxis2);

}



void MainWindow::closeEvent( QCloseEvent* event )
{
    if( port.L_isConnected() ) {
        qDebug() << "The port is connected, so the program will not close rn\n";
        QMessageBox msgBox;
        msgBox.setText("There is an active connection.");
        msgBox.setInformativeText("Are you sure you want to end this session?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();
        if( ret == QMessageBox::Yes ){
            qDebug() << "Closing the program\n" ;
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        qDebug() << "Closing the program\n" ;
        event->accept();
    }
}

/**
*   Called when the window is closed.
*   Creates and saves a backup file of logged data.
*/
MainWindow::~MainWindow()
{
    this->csvdoc.close();

    delete player;
    delete ui;
}



/**
*   Called when data was read from the port.
*   Fills a new row in the output table. Updates the graph, and any parameters shown in the GUI.
*/
void MainWindow::showRequest(const QString &req)
{

    if (req.contains('!')) {
        ui->emergencyMessageLabel->setText(req);
        if(req.contains("overheat")) {
            player->setVolume(100);
            player->play();
            ui->scoreLabel->setText("Score: " + QString::number(static_cast<double>(com.get(i_score)), 'f', 2)); //show the score precision = 2
            ui->scoreRankLabel->setText("You have earned the rating of\nProfessional Crash Test Dummy" );
        }
        return;
    }

    QByteArray ba = req.toLocal8Bit();
    char *c_str = ba.data();
    if(com.deserialize_array( c_str )) {

        if (!this->validConnection) {
            this->validConnection = true;  // String was parsed therefore the correct arduino program is uploaded
            ui->percentOnInput->setEnabled(true);
            ui->emergencyMessageLabel->clear();

            // open the csv file and give it a header
            QDir backupDir("log_files");
            if( !backupDir.exists() )
                 backupDir.mkpath(".");
            QDir::setCurrent("log_files");
            QDateTime currentTime(QDateTime::currentDateTime());
            QString dateStr = currentTime.toString("d-MMM--h-m-A");
            this->csvdoc.setFileName("..\\log_files\\" + dateStr + "-Game.csv");
            if (this->csvdoc.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&this->csvdoc);
                stream << "Time, Percent on, Temperature, Filtered Temperature, Set Point, Fan Speed\n";
            }
            else{
                qDebug() << " Failed to open  csv file  \n";
                QString errMsg = this->csvdoc.errorString();
                QFileDevice::FileError err = this->csvdoc.error();
                qDebug() << " \n ERROR msg : " << errMsg ;
                qDebug() << " \n ERROR : " << err;
            }

        }

        double time       = static_cast<double>(com.get(i_time));
        double percentOn  = static_cast<double>(com.get(i_percentOn));
        double temp       = static_cast<double>(com.get(i_temperature));
        double tempFilt   = static_cast<double>(com.get(i_tempFiltered));
        double setPoint   = static_cast<double>(com.get(i_setPoint));
        double score      = static_cast<double>(com.get(i_score));
        double avg_err    = static_cast<double>(com.get(i_avg_err));
        double fanSpeed   = static_cast<double>(com.get(i_fanSpeed));

        /*
        *  Update the output table with the last parameters read from the port.
        */
        ui->outputTable->insertRow(ui->outputTable->rowCount()); // create a new row

        // add a string of each value into each column at the last row in the outputTable
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 0, new QTableWidgetItem(QString::number( time,'f',2)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 1, new QTableWidgetItem(QString::number( percentOn,'f',2)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 2, new QTableWidgetItem(QString::number( temp,'f',2)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 3, new QTableWidgetItem(QString::number( tempFilt,'f',2)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 4, new QTableWidgetItem(QString::number( setPoint,'f',2)));
        if (!ui->outputTable->underMouse())
            ui->outputTable->scrollToBottom();   // scroll to the bottom to ensure the last value is visible

        // add each value into the excel file ( the silly math here is to format the float to have only 2 decimals )
        this->xldoc.write(ui->outputTable->rowCount(), 1,  (qRound((time*100)))/100.0);
        this->xldoc.write(ui->outputTable->rowCount(), 2,  (qRound(percentOn*100))/100.0);
        this->xldoc.write(ui->outputTable->rowCount(), 3,  (qRound(temp*100))/100.0);
        this->xldoc.write(ui->outputTable->rowCount(), 4,  (qRound(tempFilt*100))/100.0);
        this->xldoc.write(ui->outputTable->rowCount(), 5,  (qRound(setPoint*100))/100.0);
        this->xldoc.write(ui->outputTable->rowCount(), 6,  (qRound(fanSpeed*100))/100.0);

        /*
        *  Update the csv file with the last data read from the port
        */
        char file_output_buffer[200]   = "";
        snprintf(file_output_buffer, sizeof(file_output_buffer),"%6.2f,%6.2f,%6.2f,%6.2f,%6.2f,%6.2f\n",
             time,  percentOn,  temp,  tempFilt,  setPoint, fanSpeed);
        QTextStream stream(&this->csvdoc);
        stream << file_output_buffer;
        stream.flush();

        /*
        *  Show the current values from the port in the current parameters area
        */
        ui->avgerrLabel->setText( QString::number(avg_err, 'f', 2)); // precision = 2

        /*
         * After 29 minutes we show the score
         * score > 20  Accident waiting to happen.
         * 20  >= score > 16  Proud owner of a learners permit.
         * 16  >= score > 13  Control Student.
         * 13  >= score        Control Master.
        */
        // check the score to determine what the 'rankString' should be
        // todo: simplify this #p3
        double show_score_time = 29.0; // time after which we show the score
        if ( time > show_score_time) {
            ui->scoreLabel->setText("Score: " + QString::number(score, 'f', 2));  //show value of score with precision = 2
            char rankString[300];
            snprintf(rankString, sizeof(rankString), "You have earned\nthe rating of:\nAccident waiting to happen");
            if ( score <= 20.0) {
                if ( score <= 16.0) {
                    if ( score <= 13.0) {
                              snprintf(rankString, sizeof(rankString), "You have achieved\nthe rating of:\nControl Master");
                    } else {  snprintf(rankString, sizeof(rankString), "You have achieved\nthe rating of:\nControl Student") ; }
                } else {      snprintf(rankString, sizeof(rankString), "You have achieved\nthe rating of:\nProud owner\nof a learners permit") ; }
            }
            qDebug() << "rank output string: " << rankString << "\n";
            ui->scoreRankLabel->setText(rankString);
        }


        /*
        *  Place the latest values in the graph
        */
        ui->plot->graph(3)->addData( time,  percentOn);
        ui->plot->graph(2)->addData( time,  temp);
        ui->plot->graph(1)->addData( time,  tempFilt);
        ui->plot->graph(0)->addData( time,  setPoint);
        ui->plot->replot( QCustomPlot::rpQueuedReplot );
        if (ui->auto_fit_CheckBox->isChecked())
            ui->plot->rescaleAxes(); // should be in a button or somethng


    }
    else
    {
        qDebug() << "ERROR Failed to deserialize array \n";
        if (!this->validConnection)
            ui->emergencyMessageLabel->setText("Possible incorrect arduino program uploaded.");
    }
}



/**
*   Called when the user clicks the set button.
*   creates a string from the values in the textboxes and sends it to the port.
*/
void MainWindow::on_setButton_clicked()
{
    if (port.L_isConnected()) {   // we are connected so we can send the data in the textbox
        bool isNumerical = false;
        QString pOnStr = ui->percentOnInput->text();   // get string from perent on textbox
        pOnStr.remove(' ');
        if (!pOnStr.isEmpty()) {
            float pOn = pOnStr.toFloat(&isNumerical);    // convert to a float value
            if (!isNumerical) {
                // the user input is not a valid number
                QMessageBox msgBox;
                msgBox.setText("The percent on value is not numerical");
                msgBox.exec();
                ui->percentOnInput->clear();
            }
            else if ( pOn > 100 || pOn < 0 ) { // ensure the not out of range of a reasonable percent on value
                QMessageBox msgBox;
                msgBox.setText("The percent on value is out of range");
                msgBox.exec();
                ui->percentOnInput->clear();
            } else {
                // its okay
                // [percentOn,setPoint,fanSpeed,temperature,tempFiltered,time,inputVar,avg_err,score,]
                pOnStr.prepend("[");
                pOnStr.append(",,,,,,,,,]");
                emit this->response(pOnStr);  // call 'response' to send the string to the port
            }
        }
    } else {
        /*
        *  if we arent connect then emit a signal as if the user clicked the first option in the combobox
        *  and therefore a connection will be attempted.
        */
        emit this->on_portComboBox_activated(0);
    }
}



/**
*   Called by the timer ever 250 ms, until the timer is killed.
*   Checks if a connection is active, if no connection is active,
*   search for available ports and open a connection.
*/
void MainWindow::timerEvent(QTimerEvent *event)
{
    Q_UNUSED( event ) // to ignore the unused parameter warning
    if (!port.L_isConnected()) {
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        // todo: check what hapens if the socket changes. This may be an issue if we change the COM before establishing a connection  #p2
        if( ui->portComboBox->count() != portList.size()) {
            ui->portComboBox->clear();
            for(int i = 0; i < portList.size(); i ++)
            {
                QString str = portList.at(i).portName();
                if(portList.at(i).description().contains("arduino", Qt::CaseInsensitive)) {
                    str.append(" - " + portList.at(i).description());
                }
                ui->portComboBox->addItem(str);
            }
        }
    }
    else {
        killTimer(this->timerId); // no reason for the timer anymore
        ui->setButton->setText("Set");
        if( port.L_isConnected() ) {
            ui->portComboBox->setDisabled(1);
        }
    }
}



/**
*   called when the user pressed on the combobox
*   connects to the port selected.
*/
void MainWindow::on_portComboBox_activated(int index)
{
    if ( !port.L_isConnected() )
    {  // the port is not conneted yet so we should connect
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        if(portList.size() != 0)
        {
            port.openPort(portList.at(index));
        }
    }
}




/**
*   Called when the port is disconnected
*   Tells the user to manually restart the application
*/
bool MainWindow::disonnectedPopUpWindow()
{
    QMessageBox::critical(this,
                          "Error",
                          "Fatal Error, device disconnected.\n"
                          "Close and restart the application to continue.\n",
                          QMessageBox::Ok
                          );
    return true;
}


/**
 * called when the user presses enter or return
 * // thank you numbat: https://www.qtcentre.org/threads/26313-MainWindow-Button
 */
bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) {
            emit on_setButton_clicked();
            return true;
        }
    }
    return QMainWindow::event(event);
}

/**
 * Called when the user clicks the export excel file option in the dropdown menu of file.
 * Allows the user to choose where to save the file.
 */
void MainWindow::on_actionExport_Excel_File_triggered()
{
    this->excelFileName = QFileDialog::getSaveFileName(this, //parent
                                tr("Save File"), //caption
                                this->excelFileName,  // dir
                               tr("(*.xlsx)")); // filter

    qDebug() << "Saving excel file with Filename: " << this->excelFileName << "\n";

    if(!this->excelFileName.isNull()) {    // The user chose a valid filname
        this->xldoc.saveAs(this->excelFileName);
    }

}


/**
 * @brief MainWindow::on_auto_fit_CheckBox_stateChanged
 * Called when the user clicked the auto-fit checkbox, changes the default settings of the graph to fit the data.
 * @param arg1
 * arg1 == true == 1 when checked
 */
void MainWindow::on_auto_fit_CheckBox_stateChanged(int arg1)
{
    ui->plot->setInteraction(QCP::iRangeDrag, !arg1);
    ui->plot->setInteraction(QCP::iRangeZoom, !arg1);
}

/**
 * @brief MainWindow::on_zoom_xaxis_checkBox_stateChanged
 * Called when the user clicks the zoom-x checkbox, changes behavior of zooming into the x-axis
 * @param arg1
 * arg1 == true ==1 when checked
 */
void MainWindow::on_zoom_xaxis_checkBox_stateChanged(int arg1)
{
    Q_UNUSED(arg1);

    QCPAxisRect *ar = ui->plot->axisRect(0); // get the default range axis and tell it to zoom and drag relative to right side yaxis
    QCPAxis *y = ui->plot->yAxis2;
    QCPAxis *x = ui->plot->xAxis2;

    if (ui->zoom_xaxis_checkBox->isChecked())
        x = ui->plot->xAxis;
    if (ui->zoomy_checkBox->isChecked())
        y = ui->plot->yAxis;

//// *dont care about dragging  ..      ar->setRangeDragAxes(ui->plot->xAxis, ui->plot->yAxis2);
   ar->setRangeZoomAxes(x, y);
   ar->setRangeDragAxes(ui->plot->xAxis, y);
}

void MainWindow::on_zoomy_checkBox_stateChanged(int arg1)
{
    emit (on_zoom_xaxis_checkBox_stateChanged(arg1));
}


void MainWindow::on_actionAbout_triggered()
{
    About* ab = new About();
    ab->show();
}
