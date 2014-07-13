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

#ifndef _METERWIDGET_H
#define _METERWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QRect>

 class MeterWidget : public QWidget
 {
     Q_OBJECT

 public:
     MeterWidget(QWidget *parent = 0);
     ~MeterWidget();

     void setLevel(qreal value);

 protected:
     void paintEvent(QPaintEvent *event);

 private:
     qreal level;
     QPixmap pixmap;
 };


#endif /* _METERWIDGET_H */
