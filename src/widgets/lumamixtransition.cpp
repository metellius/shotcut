/*
 * Copyright (c) 2014 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lumamixtransition.h"
#include "ui_lumamixtransition.h"
#include "settings.h"
#include "mltcontroller.h"
#include <QFileDialog>
#include <QFileInfo>

static const int kLumaComboCustomIndex = 23;

LumaMixTransition::LumaMixTransition(Mlt::Producer &producer, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LumaMixTransition)
    , m_producer(producer)
{
    ui->setupUi(this);

    QScopedPointer<Mlt::Transition> transition(getTransition("luma"));
    if (transition && transition->is_valid()) {
        QString resource = transition->get("resource");
        if (!resource.isEmpty() && resource.indexOf("%luma") != -1) {
            ui->lumaCombo->setCurrentIndex(resource.midRef(resource.indexOf("%luma") + 5).left(2).toInt());
        } else if (!resource.isEmpty()) {
            ui->lumaCombo->setCurrentIndex(kLumaComboCustomIndex);
        } else {
            ui->invertCheckBox->setDisabled(true);
            ui->softnessSlider->setDisabled(true);
            ui->softnessSpinner->setDisabled(true);
        }
        ui->invertCheckBox->setChecked(transition->get_int("invert"));
        if (transition->get("softness"))
            ui->softnessSlider->setValue(qRound(transition->get_double("softness") * 100.0));
        updateCustomLumaLabel(*transition);
    }
    transition.reset(getTransition("mix"));
    if (transition && transition->is_valid()) {
        if (transition->get_int("start") == -1) {
            ui->crossfadeRadioButton->setChecked(true);
            ui->mixSlider->setDisabled(true);
            ui->mixSpinner->setDisabled(true);
        } else {
            ui->mixRadioButton->setChecked(true);
        }
        ui->mixSlider->setValue(qRound(transition->get_double("start") * 100.0));
    }
}

LumaMixTransition::~LumaMixTransition()
{
    delete ui;
}

void LumaMixTransition::on_invertCheckBox_clicked(bool checked)
{
    QScopedPointer<Mlt::Transition> transition(getTransition("luma"));
    if (transition && transition->is_valid()) {
        transition->set("invert", checked);
        MLT.refreshConsumer();
    }
}

void LumaMixTransition::on_softnessSlider_valueChanged(int value)
{
    QScopedPointer<Mlt::Transition> transition(getTransition("luma"));
    if (transition && transition->is_valid()) {
        transition->set("softness", value / 100.0);
        MLT.refreshConsumer();
    }
}

void LumaMixTransition::on_crossfadeRadioButton_clicked()
{
    QScopedPointer<Mlt::Transition> transition(getTransition("mix"));
    if (transition && transition->is_valid()) {
        transition->set("start", -1);
    }
    ui->mixSlider->setDisabled(true);
    ui->mixSpinner->setDisabled(true);
}

void LumaMixTransition::on_mixRadioButton_clicked()
{
    QScopedPointer<Mlt::Transition> transition(getTransition("mix"));
    if (transition && transition->is_valid()) {
        transition->set("start", ui->mixSlider->value() / 100.0);
    }
    ui->mixSlider->setEnabled(true);
    ui->mixSpinner->setEnabled(true);
}

void LumaMixTransition::on_mixSlider_valueChanged(int value)
{
    QScopedPointer<Mlt::Transition> transition(getTransition("mix"));
    if (transition && transition->is_valid()) {
        transition->set("start", value / 100.0);
    }
}

Mlt::Transition *LumaMixTransition::getTransition(const QString &name)
{
    QScopedPointer<Mlt::Service> service(m_producer.producer());
    while (service && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition transition(*service);
            if (name == transition.get("mlt_service"))
                return new Mlt::Transition(transition);
            else if (name == "luma" && QString("movit.luma_mix") == transition.get("mlt_service"))
                return new Mlt::Transition(transition);
        }
        service.reset(service->producer());
    }
    return 0;
}

void LumaMixTransition::updateCustomLumaLabel(Mlt::Transition &transition)
{
    QString resource = transition.get("resource");
    if (resource.isEmpty() || resource.indexOf("%luma") != -1) {
        ui->customLumaLabel->hide();
        ui->customLumaLabel->setToolTip(QString());
    } else if (!resource.isEmpty()) {
        ui->customLumaLabel->setText(QFileInfo(transition.get("resource")).fileName());
        ui->customLumaLabel->setToolTip(transition.get("resource"));
        ui->customLumaLabel->show();
    }
}

void LumaMixTransition::on_lumaCombo_activated(int index)
{
    ui->invertCheckBox->setEnabled(index > 0);
    ui->softnessSlider->setEnabled(index > 0);
    ui->softnessSpinner->setEnabled(index > 0);

    QScopedPointer<Mlt::Transition> transition(getTransition("luma"));
    if (transition && transition->is_valid()) {
        if (index == 0) {
            transition->set("resource", "");
        } else if (index == kLumaComboCustomIndex) {
            // Custom file
            QString path = Settings.openPath();
#ifdef Q_OS_MAC
            path.append("/*");
#endif
            QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), path,
                tr("Images (*.bmp *.jpeg *.jpg *.pgm *.png *.svg *.tga *.tif *.tiff);;All Files (*)"));
            activateWindow();
            if (!filename.isEmpty())
                transition->set("resource", filename.toUtf8().constData());
        } else {
            transition->set("resource", QString("%luma%1.pgm").arg(index, 2, 10, QChar('0')).toLatin1().constData());
        }
        if (qstrcmp(transition->get("resource"), "")) {
            transition->set("progressive", 1);
            transition->set("invert", ui->invertCheckBox->isChecked());
            transition->set("softness", ui->softnessSlider->value() / 100.0);
        }
        updateCustomLumaLabel(*transition);
        MLT.refreshConsumer();
    }
}
