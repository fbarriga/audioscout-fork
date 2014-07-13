/*
    Audio Scout - audio content indexing software
    Copyright (C) 2010  D. Grant Starkweather & Evan Klinger
    
    Audio Scout is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    D. Grant Starkweather - dstarkweather@phash.org
    Evan Klinger          - eklinger@phash.org
*/

#include "MeterWidget.h"


MeterWidget::MeterWidget(QWidget *parent):QWidget(parent){
    //    setBackgroundRole(QPalette::Base);
    //setAutoFillBackground(false);

    level = 0;
    setMinimumHeight(10);
    setMinimumWidth(150);

}

MeterWidget::~MeterWidget(){

}

void MeterWidget::setLevel(qreal value){
    level = value;
}

void MeterWidget::paintEvent(QPaintEvent *event){
    QPainter painter(this);

     painter.setPen(Qt::black);
     painter.drawRect(QRect(painter.viewport().left(),
                            painter.viewport().top(),
                            painter.viewport().right(),
                            painter.viewport().bottom()));
     if (level == 0.0)
         return;

     painter.setPen(Qt::red);

     int barLength = (painter.viewport().right())-(painter.viewport().left());
     int meterPos = (int)((float)barLength*level);
     int barWidth = (painter.viewport().bottom()) - (painter.viewport().top());
     for (int i = 0; i < barWidth; ++i) {
         int x1 = painter.viewport().left();
         int y1 = painter.viewport().top()+i;
         int x2 = painter.viewport().left() + meterPos;
         int y2 = painter.viewport().top()+i;

         painter.drawLine(QPoint(x1, y1),QPoint(x2, y2));
     }

}
