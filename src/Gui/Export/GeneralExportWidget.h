// Copyright (C) 2012-2016 The VPaint Developers.
// See the COPYRIGHT file at the top-level directory of this distribution
// and at https://github.com/dalboris/vpaint/blob/master/COPYRIGHT
//
// This file is part of VPaint, a vector graphics editor. It is subject to the
// license terms and conditions in the LICENSE.MIT file found in the top-level
// directory of this distribution and at http://opensource.org/licenses/MIT

#ifndef GENERALEXPORTSETTINGSWIDGET_H
#define GENERALEXPORTSETTINGSWIDGET_H

#include <QWidget>

class GeneralExportWidget: public QWidget
{
private:
    Q_OBJECT
    Q_DISABLE_COPY(GeneralExportWidget)

public:
    GeneralExportWidget(QWidget * parent = nullptr);
};

#endif // GENERALEXPORTSETTINGSWIDGET_H
