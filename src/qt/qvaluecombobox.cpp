// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qvaluecombobox.h"

QValueComboBox::QValueComboBox(QWidget *parent) :
        QComboBox(parent), role(Qt::UserRole)
{
    connect(this, SIGNAL(currentApollonChanged(int)), this, SLOT(handleSelectionChanged(int)));
}

QVariant QValueComboBox::value() const
{
    return itemData(currentApollon(), role);
}

void QValueComboBox::setValue(const QVariant &value)
{
    setCurrentApollon(findData(value, role));
}

void QValueComboBox::setRole(int role)
{
    this->role = role;
}

void QValueComboBox::handleSelectionChanged(int xap)
{
    Q_EMIT valueChanged();
}
