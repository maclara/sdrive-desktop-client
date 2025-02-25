/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QDir>
#include <QFileDialog>
#include <QUrl>
#include <QTimer>
#include <QPushButton>
#include <QMessageBox>
#include <QSsl>
#include <QSslCertificate>

#include "QProgressIndicator.h"

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudsetuppage.h"
#include "wizard/owncloudconnectionmethoddialog.h"
#include "../3rdparty/certificates/p12topem.h"
#include "theme.h"
#include "account.h"

namespace OCC
{

OwncloudSetupPage::OwncloudSetupPage(QWidget *parent)
  : QWizardPage(),
    _ui(),
    _oCUrl(),
    _ocUser(),
    _authTypeKnown(false),
    _checking(false),
    _authType(WizardCommon::HttpCreds),
    _progressIndi(new QProgressIndicator (this))
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup %1 server").arg(theme->appNameGUI())));

    registerField( QLatin1String("OCUrl*"), _ui.leUrl );

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();

    setupCustomization();

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(slotUrlChanged(QString)));
    connect(_ui.leUrl, SIGNAL(editingFinished()), SLOT(slotUrlEditFinished()));

    addCertDial = new AddCertificateDialog(this);
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);
    connect(_ocWizard,SIGNAL(needCertificate()),this,SLOT(slotAskSSLClientCertificate()));
}

void OwncloudSetupPage::setServerUrl( const QString& newUrl )
{
    _oCUrl = newUrl;
    if( _oCUrl.isEmpty() ) {
        _ui.leUrl->clear();
        return;
    }

    _ui.leUrl->setText( _oCUrl );
}

void OwncloudSetupPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia( Theme::oCSetupTop );
    if( !variant.isNull() ) {
        WizardCommon::setupCustomMedia( variant, _ui.topLabel );
    }

    variant = theme->customMedia( Theme::oCSetupBottom );
    WizardCommon::setupCustomMedia( variant, _ui.bottomLabel );
}

// slot hit from textChanged of the url entry field.
void OwncloudSetupPage::slotUrlChanged(const QString& url)
{
    _authTypeKnown = false;

    QString newUrl = url;
    if (url.endsWith("index.php")) {
        newUrl.chop(9);
    }
    if (url.endsWith("remote.php/webdav")) {
        newUrl.chop(17);
    }
    if (url.endsWith("remote.php/webdav/")) {
        newUrl.chop(18);
    }
    if (newUrl != url) {
        _ui.leUrl->setText(newUrl);
    }

    if (url.startsWith(QLatin1String("http://"))) {
        _ui.urlLabel->setPixmap(QPixmap(Theme::hidpiFileName(":/client/resources/lock-http.png")));
        _ui.urlLabel->setToolTip(tr("This url is NOT secure as it is not encrypted.\n"
                                    "It is not advisable to use it."));
    } else {
        _ui.urlLabel->setPixmap(QPixmap(Theme::hidpiFileName(":/client/resources/lock-https.png")));
        _ui.urlLabel->setToolTip(tr("This url is secure. You can use it."));
    }
}

void OwncloudSetupPage::slotUrlEditFinished()
{
    QString url = _ui.leUrl->text();
    if (QUrl(url).isRelative()) {
        // no scheme defined, set one
        url.prepend("https://");
    }
    _ui.leUrl->setText(url);
}

bool OwncloudSetupPage::isComplete() const
{
    return !_ui.leUrl->text().isEmpty() && !_checking;
}

void OwncloudSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _authTypeKnown = false;
    _checking  = false;

    QAbstractButton *nextButton = wizard()->button(QWizard::NextButton);
    QPushButton *pushButton = qobject_cast<QPushButton*>(nextButton);
    if (pushButton)
        pushButton->setDefault(true);

    _ui.leUrl->setFocus();
    _ui.leUrl->setText(Theme::instance()->overrideServerUrl());
}

bool OwncloudSetupPage::urlHasChanged()
{
    bool change = false;
    const QChar slash('/');

    QUrl currentUrl( url() );
    QUrl initialUrl( _oCUrl );

    QString currentPath = currentUrl.path();
    QString initialPath = initialUrl.path();

    // add a trailing slash.
    if( ! currentPath.endsWith( slash )) currentPath += slash;
    if( ! initialPath.endsWith( slash )) initialPath += slash;

    if( currentUrl.host() != initialUrl.host() ||
        currentUrl.port() != initialUrl.port() ||
            currentPath != initialPath ) {
        change = true;
    }

    return change;
}

int OwncloudSetupPage::nextId() const
{
    return WizardCommon::Page_HttpCreds;
}

QString OwncloudSetupPage::url() const
{
    QString url;

    if (_ui.leUrl->text().isEmpty())
        url = Theme::instance()->overrideServerUrl();
    else
        url = _ui.leUrl->text().simplified();

    return url;
}

bool OwncloudSetupPage::validatePage()
{
    if( ! _authTypeKnown) {
        setErrorString(QString(), false);
        _checking = true;
        startSpinner ();
        emit completeChanged();

        emit determineAuthType(url());
        return false;
    } else {
        // connecting is running
        stopSpinner();
        _checking = false;
        emit completeChanged();
        return true;
    }
}

void OwncloudSetupPage::setAuthType (WizardCommon::AuthType type)
{
  _authTypeKnown = true;
  _authType = type;
  stopSpinner();
}

void OwncloudSetupPage::setErrorString( const QString& err, bool retryHTTPonly )
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        if (retryHTTPonly) {
            QUrl url(_ui.leUrl->text());
            if (url.scheme() == "https") {
                // Ask the user how to proceed when connecting to a https:// URL fails.
                // It is possible that the server is secured with client-side TLS certificates,
                // but that it has no way of informing the owncloud client that this is the case.

                OwncloudConnectionMethodDialog dialog;
                dialog.setUrl(url);
                int retVal = dialog.exec();

                switch (retVal) {
                case OwncloudConnectionMethodDialog::No_TLS:
                    {
                        url.setScheme("http");
                        _ui.leUrl->setText(url.toString());
                        // skip ahead to next page, since the user would expect us to retry automatically
                        wizard()->next();
                    }
                    break;
                case OwncloudConnectionMethodDialog::Client_Side_TLS:
                    slotAskSSLClientCertificate();
                    break;
                case OwncloudConnectionMethodDialog::Back:
                default:
                    // No-op.
                    break;
                }
            }
        }

        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _checking = false;
    emit completeChanged();
    stopSpinner();
}

void OwncloudSetupPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudSetupPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

void OwncloudSetupPage::setConfigExists(  bool config )
{
    if (config == true) {
        setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Update %1 server")
                                                         .arg(Theme::instance()->appNameGUI())));
    }
}


void OwncloudSetupPage::slotAskSSLClientCertificate()
{
    addCertDial->show();
    connect(addCertDial, SIGNAL(accepted()),this,SLOT(slotCertificateAccepted()));
}

QString subjectInfoHelper(const QSslCertificate& cert, const QByteArray &qa)
{
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    return cert.subjectInfo(qa);
#else
    return cert.subjectInfo(qa).join(QLatin1Char('/'));
#endif
}

//called during the validation of the client certificate.
void OwncloudSetupPage::slotCertificateAccepted()
{
    QSslCertificate sslCertificate;

    resultP12ToPem certif = p12ToPem(addCertDial->getCertificatePath().toStdString() , addCertDial->getCertificatePasswd().toStdString());
    if(certif.ReturnCode){
        QString s = QString::fromStdString(certif.Certificate);
        QByteArray ba = s.toLocal8Bit();

        QList<QSslCertificate> sslCertificateList = QSslCertificate::fromData(ba, QSsl::Pem);
        sslCertificate = sslCertificateList.takeAt(0);

        _ocWizard->ownCloudCertificate = ba;
        _ocWizard->ownCloudPrivateKey = certif.PrivateKey.c_str();
        _ocWizard->ownCloudCertificatePath = addCertDial->getCertificatePath();
        _ocWizard->ownCloudCertificatePasswd = addCertDial->getCertificatePasswd();

        AccountPtr acc = _ocWizard->account();
        acc->setCertificate(_ocWizard->ownCloudCertificate, _ocWizard->ownCloudPrivateKey);
        addCertDial->reinit();
        validatePage();
    } else {
        QString message;
        message = certif.Comment.c_str();
        addCertDial->showErrorMessage(message);
        addCertDial->show();
    }
}

OwncloudSetupPage::~OwncloudSetupPage()
{
    delete addCertDial;
}

} // namespace OCC
