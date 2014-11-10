/***************************************************************************
                           ngspicesimdialog.cpp
                             ----------------
    begin                : Sun Nov 9 2014
    copyright            : (C) 2014 by Vadim Kuznetsov
    email                : ra3xdh@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ngspicesimdialog.h"

NgspiceSimDialog::NgspiceSimDialog(Schematic *sch,QWidget *parent) :
    QDialog(parent)
{
    Sch = sch;

    ngspice = new QProcess(this);
    ngspice->setProcessChannelMode(QProcess::MergedChannels);
    connect(ngspice,SIGNAL(finished(int)),this,SLOT(slotProcessNgSpiceOutput(int)));
    connect(this,SIGNAL(destroyed()),this,SLOT(killThemAll()));

    buttonSimulate = new QPushButton(tr("Simulate"),this);
    connect(buttonSimulate,SIGNAL(clicked()),this,SLOT(slotSimulate()));
    buttonStopSim = new QPushButton(tr("Stop"),this);
    connect(buttonStopSim,SIGNAL(clicked()),this,SLOT(reject()));
    connect(buttonStopSim,SIGNAL(clicked()),this,SLOT(killThemAll()));
    buttonStopSim->setEnabled(false);

    QGroupBox *grp1 = new QGroupBox(tr("Simulation console"),this);
    QVBoxLayout *vbl1 = new QVBoxLayout;
    editSimConsole = new QTextEdit(this);
    editSimConsole->setReadOnly(true);
    vbl1->addWidget(editSimConsole);
    grp1->setLayout(vbl1);

    QVBoxLayout *vl_top = new QVBoxLayout;
    vl_top->addWidget(grp1);
    QHBoxLayout *hl1 = new QHBoxLayout;
    hl1->addWidget(buttonSimulate);
    hl1->addWidget(buttonStopSim);
    vl_top->addLayout(hl1);
    this->setLayout(vl_top);
}

NgspiceSimDialog::~NgspiceSimDialog()
{
    killThemAll();
}

void NgspiceSimDialog::slotSimulate()
{
    int num=0;
    sims.clear();
    vars.clear();
    QString tmp_path = QDir::homePath()+"/.qucs/spice4qucs.cir";
    QFile spice_file(tmp_path);
    if (spice_file.open(QFile::WriteOnly)) {
        QTextStream stream(&spice_file);
        createSpiceNetlist(stream,num,sims,vars,output_files);
        spice_file.close();
    }
    qDebug()<<sims<<vars;

    startNgSpice(tmp_path);
    /*QString old_dir = QDir::currentPath(); // Execute ngspice
    QDir::setCurrent(QDir::homePath()+"/.qucs");
    QProcess::execute("ngspice "+tmp_path);
    QDir::setCurrent(old_dir);*/
}

void NgspiceSimDialog::startNgSpice(QString netlist)
{
    buttonStopSim->setEnabled(true);
    ngspice->setWorkingDirectory(QDir::convertSeparators(QDir::homePath()+"/.qucs/spice4qucs"));
    ngspice->start("ngspice "+ netlist);
}

void NgspiceSimDialog::killThemAll()
{
    buttonStopSim->setEnabled(false);
    if (ngspice->state()!=QProcess::NotRunning) {
        ngspice->kill();
    }
}

void NgspiceSimDialog::slotProcessNgSpiceOutput(int exitCode)
{
    buttonStopSim->setEnabled(false);
    QString out = ngspice->readAllStandardOutput();
    editSimConsole->clear();
    editSimConsole->append(out);
    // Set temporary safe output name
    convertToQucsData(Sch->DocName+".s4q.dat");
}

void NgspiceSimDialog::convertToQucsData(const QString &qucs_dataset)
{
    QFile dataset(qucs_dataset);
    if (dataset.open(QFile::WriteOnly)) {
        QTextStream ds_stream(&dataset);

        ds_stream<<"Qucs Dataset "PACKAGE_VERSION">\n";

        QString sim,indep;
        QStringList indep_vars;
        /*foreach(sim,sims) { // extract independent vars from simulations list
            if (sim=="tran") {
                indep_vars<<"time";
            } else if (sim=="ac") {
                indep_vars<<"frequency";
            }
            ds_stream<<QString("<indep %1>\n").arg(indep_vars.last());
            ds_stream<<"</indep>\n";
        }*/

        /*foreach (indep,indep_vars) { // form dependent vars
            QString var;
            foreach (var,vars) {
                ds_stream<<QString("<dep %1.Vt %2>\n").arg(var).arg(indep);
                ds_stream<<"</dep>\n";
            }
        }*/

        /*QString ng_spice_output_filename;
        foreach(ng_spice_output_filename,output_files) {
            QFile ofile(ng_spice_output_filename);
            if (ofile.open(QFile::ReadOnly)) {

                ofile.close();
            }
        }*/

        dataset.close();
    }
}


void NgspiceSimDialog::createSpiceNetlist(QTextStream& stream, int NumPorts,QStringList& simulations, QStringList& vars,
                                      QStringList &outputs)
{
    if(!Sch->prepareSpiceNetlist(stream)) return; // Unable to perform ngspice simulation

    QString s;
    for(Component *pc = Sch->DocComps.first(); pc != 0; pc = Sch->DocComps.next()) {
      if(Sch->isAnalog &&
         !(pc->isSimulation) &&
         !(pc->isProbe)) {
        s = pc->getSpiceNetlist();
        stream<<s;
      }
    }

    // determine which simulations are in use
    simulations.clear();
    for(Component *pc = Sch->DocComps.first(); pc != 0; pc = Sch->DocComps.next()) {
       if(pc->isSimulation) {
           s = pc->getSpiceNetlist();
           QString sim_typ = pc->Model;
           if (sim_typ==".AC") simulations.append("ac");
           if (sim_typ==".TR") simulations.append("tran");
           if (sim_typ==".DC") simulations.append("dc");
           stream<<s;
       }
    }

    // set variable names for named nodes and wires
    vars.clear();
    for(Node *pn = Sch->DocNodes.first(); pn != 0; pn = Sch->DocNodes.next()) {
      if(pn->Label != 0) {
          if (!vars.contains(pn->Label->Name)) {
              vars.append(pn->Label->Name);
          }
      }
    }
    for(Wire *pw = Sch->DocWires.first(); pw != 0; pw = Sch->DocWires.next()) {
      if(pw->Label != 0) {
          if (!vars.contains(pw->Label->Name)) {
              vars.append(pw->Label->Name);
          }
      }
    }
    vars.sort();
    qDebug()<<vars;

    stream<<".control\n"          //execute simulations
          <<"set filetype=ascii\n"
          <<"run\n";

    QFileInfo inf(Sch->DocName);
    QString basenam = inf.baseName();

    QString sim;                 // see results
    outputs.clear();
    foreach (sim,simulations) {
        QString nod;
        foreach (nod,vars) {
            QString filename = QString("%1_%2_%3.txt").arg(basenam).arg(sim).arg(nod);
            QString write_str = QString("write %1 %2.v(%3)\n").arg(filename).arg(sim).arg(nod);
            stream<<write_str;
            outputs.append(filename);
        }
        stream<<endl;
    }

    stream<<"exit\n"
          <<".endc\n";

    stream<<".END\n";

}
