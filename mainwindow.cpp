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
    // timer is used to repeatedly check for port data until we are connected
    this->timerId = startTimer(1000);
    // indicates when we are connected to the port AND the correct arduino program is being run
    this->validConnection = false;
    // initialize the input vector to hold the input values from the port
    this->inputs = std::vector<float>(this->numInputs);
    // have the table resize with the window
    ui->outputTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   // have the table resize with the window
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

    this->excelFileName = "excelFile.xlsx";
    this->csvFileName   = "data.csv";

    if (QFile::exists(this->excelFileName)) {
        QFile oldFile(this->excelFileName);
        oldFile.remove();
    }

    // give the excel file column headers
    this->xldoc.write( 1 , 1, "Time");
    this->xldoc.write( 1 , 2, "Percent On");
    this->xldoc.write( 1 , 3, "Temperature");
    this->xldoc.write( 1 , 4, "Filtered Temperature");
    this->xldoc.write( 1 , 5, "Set Point");

    // open the csv file and give it a header
    this->csvdoc.setFileName(this->csvFileName);
    if (this->csvdoc.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&this->csvdoc);
        stream << "Time, perent on, Temp, Temp filtered, Set Point\n";
    }
    else{
        qDebug() << " Failed to open data.csv \n";
    }


    /*
    * Set up the live plot on the GUI/
    * The set point must have a special scatterstyle so it doesnt connect the lines
    */
    ui->plot->addGraph();
    ui->plot->graph(0)->setName("Set Point");
    ui->plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor("orange"), 5));
    ui->plot->graph(0)->setPen(QPen(Qt::white)); // so we dont see the line connecting the dots

    ui->plot->addGraph();
    ui->plot->graph(1)->setName("Temperature");
    ui->plot->graph(1)->setPen(QPen(Qt::green));

    ui->plot->addGraph();
    ui->plot->graph(2)->setName("Temperature Filtered");
    ui->plot->graph(2)->setPen(QPen(Qt::blue));

    ui->plot->addGraph();
    ui->plot->graph(3)->setName("Percent Heater on");
    ui->plot->graph(3)->setPen(QPen(QColor("purple"))); // line color for the first graph
    ui->plot->graph(3)->setValueAxis(ui->plot->yAxis2);

    ui->plot->xAxis2->setVisible(true);  // show x ticks at top
    ui->plot->xAxis2->setVisible(false); // dont show labels at top
    ui->plot->yAxis2->setVisible(true);  // right y axis labels
    ui->plot->yAxis2->setTickLabels(true);  // show y ticks on right side for % on

    ui->plot->yAxis2->setLabel("Heater [%]");
    ui->plot->yAxis->setLabel("Temperature [C]");
    ui->plot->xAxis->setLabel("Time [min]");

    // setup the legend
    ui->plot->legend->setVisible(true);
    ui->plot->legend->setFont(QFont("Helvetica", 8));
    ui->plot->legend->setRowSpacing(-4);  // less space between words
    ui->plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);

}



/**
*   Called when the window is closed.
*   Creates and saves a backup file of logged data.
*/
MainWindow::~MainWindow()
{
    if( this->port.L_isConnected())
    {
        // ensure a backupfile directory exists, create one if it doesnt
        QDir backupDir("backupFiles");
        if( !backupDir.exists() )
             backupDir.mkpath(".");
        QDir::setCurrent("backupFiles");

        QDateTime currentTime(QDateTime::currentDateTime());
        QString dateStr = currentTime.toString("d-MMM--h-m-A");  // create a date title for the backup file
        dateStr.append(".csv"); // the 's' cant be used when formatting the time string
        this->csvdoc.copy(dateStr); // save a backup file of the csv file
    }
    this->csvdoc.close();


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
        return;
    }

    if (this->deserializeArray(req.toStdString().c_str(), this->numInputs, this->inputs)) {

        if (!this->validConnection) {
            this->validConnection = true;  // String was parsed therefore the correct arduino program is uploaded
            ui->percentOnInput->setEnabled(true);
            ui->emergencyMessageLabel->clear();
        }

        /*
        *  Update the output table with the last parameters read from the port.
        */
        ui->outputTable->insertRow(ui->outputTable->rowCount()); // create a new row

        // add a string of each value into each column at the last row in the outputTable
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 0, new QTableWidgetItem(QString::number(inputs[i_time],'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 1, new QTableWidgetItem(QString::number(inputs[i_percentOn],'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 2, new QTableWidgetItem(QString::number(inputs[i_temperature],'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 3, new QTableWidgetItem(QString::number(inputs[i_tempFiltered],'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 4, new QTableWidgetItem(QString::number(inputs[i_setPoint],'g',3)));
        if (!ui->outputTable->underMouse())
            ui->outputTable->scrollToBottom();   // scroll to the bottom to ensure the last value is visible

        // add each value into the excel file
        this->xldoc.write(ui->outputTable->rowCount(), 1, inputs[i_time]);
        this->xldoc.write(ui->outputTable->rowCount(), 2, inputs[i_percentOn]);
        this->xldoc.write(ui->outputTable->rowCount(), 3, inputs[i_temperature]);
        this->xldoc.write(ui->outputTable->rowCount(), 4, inputs[i_tempFiltered]);
        this->xldoc.write(ui->outputTable->rowCount(), 5, inputs[i_setPoint]);
        this->xldoc.saveAs(this->excelFileName); // save the doc in case we crash

        /*
        *  Update the csv file with the last data read from the port
        */
        char file_output_buffer[200]   = "";
        snprintf(file_output_buffer, sizeof(file_output_buffer),"%6.2f,%6.2f,%6.2f,%6.2f,%6.2f\n",
            inputs[i_time], inputs[i_percentOn], inputs[i_temperature], inputs[i_tempFiltered], inputs[i_setPoint]);
        QTextStream stream(&this->csvdoc);
        stream << file_output_buffer;
        stream.flush();


        /*
        *  Show the current values from the port in the current parameters area
        */
        float show_error_and_inputVar_time = 18.0; // time after which we want to show average error and input variance
        float show_score_time = 29.0; // time after which we show the score
        if( inputs[i_time] > show_error_and_inputVar_time ) // only show the input variance after input exclusion time
        {
            ui->avgerrLabel->setNum(inputs[i_avg_err]);
            ui->inputVarLabel->setNum(inputs[i_input_var]);
        }
        if ( inputs[i_time] > show_score_time )
            ui->scoreLabel->setNum(inputs[i_score]);


        /*
         * After 29 minutes we show the score
         * score > 50         professional crash test dummy.
         * 50 >= score > 20  Accident waiting to happen.
         * 20  >= score > 16  Proud owner of a learners permit.
         * 16  >= score > 13  Control Student.
         * 13  >= score        Control Master.
        */
        // check the score to determine what the 'rankString' should be
        // todo: simplify this #p3
        if (inputs[i_time] > 29.0) {
            char rankString[200];
            snprintf(rankString, sizeof(rankString), "Professional Crash test dummy\n");
            if (inputs[i_score] <= 50.0) {
                if (inputs[i_score] <= 20.0) {
                    if (inputs[i_score] <= 16.0) {
                        if (inputs[i_score] <= 13.0) {
                                  snprintf(rankString, sizeof(rankString), "Control Master\n");
                        } else {  snprintf(rankString, sizeof(rankString), "Control Student\n") ; }
                    } else {      snprintf(rankString, sizeof(rankString), "Proud owner of a learners permit\n") ; }
                } else {          snprintf(rankString, sizeof(rankString), "Accident waiting to happen\n") ; }
            }
            qDebug() << "rank output string: " << rankString << "\n";
            ui->scoreRankLabel->setText(rankString);
        }


        /*
        *  Place the latest values in the graph
        */
        ui->plot->graph(0)->addData(inputs[i_time], inputs[i_setPoint]);
        ui->plot->graph(1)->addData(inputs[i_time], inputs[i_temperature]);
        ui->plot->graph(2)->addData(inputs[i_time], inputs[i_tempFiltered]);
        ui->plot->graph(3)->addData(inputs[i_time], inputs[i_percentOn]);
        ui->plot->replot( QCustomPlot::rpQueuedReplot );
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
                pOnStr.prepend("[");
                pOnStr.append("]");
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
    if (!port.L_isConnected()) {
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        // todo: check what hapens if the socket changes. This may be an issue if we change the COM before establishing a connection  #p2
        if( ui->portComboBox->count() != portList.size()) {
            ui->portComboBox->clear();
            for(int i = 0; i < portList.size(); i ++)
            {
               ui->portComboBox->addItem(portList.at(i).portName());
            }
        }
    }
    else {
        killTimer(this->timerId); // no reason for the timer anymore
        ui->setButton->setText("Set");
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
*   Called after new data is recieved from the port to parse the string recieved.
*   fills 'output' of five 'output_size' with the vlues in 'input' string.
*/
bool MainWindow::deserializeArray(const char* const input, unsigned int output_size,  std::vector<float> &output)
{
    /*
    Ensure that the input string has the correct format and number of numbers to be parsed
    */
    const char*  p = input;
    unsigned int num_commas     = 0;
    unsigned int num_brackets   = 0;
    unsigned int num_values     = 0;

    while (*p)
    {
      if (*p == '[') { num_brackets++;
      } else if ( *p == ']' ) {num_brackets++; num_values++;
      } else if ( *p == ',' ) {num_commas++; num_values++;
      } p++;
    }
    if (num_brackets != 2) {
        qDebug() << "(M) Parse error, not valid array\n";
        return false;
    }
    if (num_values != output_size) {
        qDebug() << "(M) Parse error, input size incorrect\n";
        return false;
    }


    char* pEnd;
    p = input + 1;
    for ( unsigned int i = 0; i < output_size; i ++ )
    {

        bool is_a_number = false;
        const char* nc = p; // nc will point to the next comma or the closing bracket
        while(*nc != ',' && *nc != ']' && *nc)
        {
            if ( (int)*nc >= 48 && (int)*nc <= 57 )
                is_a_number = true;
            nc++;
        }
        float num_ = output[i] = strtof(p, &pEnd); // strtof can returns nan when parsing nans,
           // strod returns 0 when parsing nans
        p = pEnd;
        if ( is_a_number || num_ == NAN)
            output[i] = num_;
        while (*p != ',' && *p != ']' && *p)
            p++;
        p++;
   }
   p = input;
   return true;
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
}
