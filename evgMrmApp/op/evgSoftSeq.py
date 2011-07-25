#! /usr/bin/env python

from PyQt4 import QtCore as core
from PyQt4.QtCore import *
from PyQt4 import QtGui as gui
from PyQt4.QtGui import *
import sys
import os
from subprocess import *
from PyQt4.QtCore import SIGNAL

import cothread
from cothread.catools import *

from ui_evgsoftseq import Ui_EvgSoftSeq

class evgSoftSeq(gui.QMainWindow):	
    def __init__(self, argv, parent=None):
        gui.QMainWindow.__init__(self, parent)
    
        self.ui = Ui_EvgSoftSeq()
        self.ui.setupUi(self)
        self.srcText=None

        labels = ["Event Code", "Timestamp"]
        self.ui.tableWidget.setHorizontalHeaderLabels(labels)
	
        self.arg1 = argv[1]
        self.arg2 = argv[2]
        self.arg3 = argv[3]

        self.EGU_Resolution = 0
        self.Ticks_Resolution = 0
	
        heading = self.arg2 + " " + self.arg3
        self.ui.label_Heading.setText(heading)

        self.connect(self.ui.pb_setSequence, SIGNAL("clicked()"), self.setSequence)
        self.connect(self.ui.rb_TsInpModeEGU, SIGNAL("clicked()"), self.setTsInpModeEGU)
        self.connect(self.ui.rb_TsInpModeTicks, SIGNAL("clicked()"), self.setTsInpModeTicks)
	
        pv = self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}Timestamp-SP.INP"
        camonitor(pv, self.cb_TimestampInp)

        pv = self.arg1 + "{" + self.arg2  + "-EvtClk}Frequency-RB"
        camonitor(pv, self.cb_EvtClkFreq)

        pv = self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "TsInpMode-RB"
        if(caget(pv)):
            self.ui.rb_TsInpModeTicks.setChecked(1);
        else:
            self.ui.rb_TsInpModeEGU.setChecked(1);

        self.sequenceRB()
	

    def sequenceRB(self):
        pvEC = self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "EvtCode-RB"
        valueEC = caget(pvEC)

        pvTS = self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "Timestamp-RB"
        valueTS = caget(pvTS)

        if valueEC.ok == True & valueTS.ok == True:
            for x in range(self.ui.tableWidget.rowCount()):
                if(valueEC[x]==127):
                    break
                item = QTableWidgetItem(QString.number(valueEC[x]))
                self.ui.tableWidget.setItem(x, 0, item)
                item = QTableWidgetItem(QString.number(valueTS[x], "G", 14))
                self.ui.tableWidget.setItem(x, 1, item)


    def setTsInpModeEGU(self):
        self.setTsInpMode()
        self.ui.label_tsResolution.setText( '%e Seconds'%(self.EGU_Resolution) )
        self.sequenceRB()

    def setTsInpModeTicks(self):
        self.setTsInpMode()
        self.ui.label_tsResolution.setText( '%e Seconds'%(self.Ticks_Resolution) )
        self.sequenceRB()


    def cb_TimestampInp(self, value):
        value = value[value.find('C')+1:value.find(' ')]
        self.EGU_Resolution = 1.0/pow(10, int(value))
        if self.ui.rb_TsInpModeEGU.isChecked():
            self.setTsInpModeEGU()

    def cb_EvtClkFreq(self, value):
        print value
        self.Ticks_Resolution = 1.0/(value*1000000)
        if self.ui.rb_TsInpModeTicks.isChecked():
            self.setTsInpModeTicks()

  		
    def setSequence(self):
        self.setEvtCode()
        self.setTimestamp()


    def setTsInpMode(self):
        pv =  self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "TsInpMode-Sel"
        if self.ui.rb_TsInpModeEGU.isChecked():
            caput(pv, "EGU")
        else:
            caput(pv, "TICKS")
	

    def setEvtCode(self):
        args = []
        for x in range(self.ui.tableWidget.rowCount()): 
            item = self.ui.tableWidget.item(x,0)
            if item == None:
                break
            else:
                (val, OK) = item.text().toInt()
 
            if val <= 0 or val > 255:
                break
            args.append(val)
	  
        pv =  self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "EvtCode-SP"
        caput(pv, args)

      	
    def setTimestamp(self):
        args = []
        for x in range(self.ui.tableWidget.rowCount()):
            item = self.ui.tableWidget.item(x,1)
            if item == None:
                break
            else:
                (val, OK) = item.text().toDouble()
		
            if val == 0 and x > 0:
                break
            args.append(val)
	  
        pv =  self.arg1 + "{" + self.arg2 + "-" + self.arg3 + "}" + "Timestamp-SP"
        caput(pv, args)

	
if __name__ == '__main__':
    app = cothread.iqt(argv = sys.argv)
    softSeq = evgSoftSeq(sys.argv)
    softSeq.show()
    cothread.WaitForQuit()
  
