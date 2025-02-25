/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "account.h"
#include "cookiejar.h"
#include "theme.h"
#include "networkjobs.h"
#include "configfile.h"
#include "accessmanager.h"
#include "owncloudtheme.h"
#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"
#include "../3rdparty/certificates/p12topem.h"

#include <QSettings>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QNetworkCookieJar>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSslKey>

namespace OCC {

static const char urlC[] = "url";
static const char authTypeC[] = "authType";
static const char userC[] = "user";
static const char caCertsKeyC[] = "CaCertificates";

AccountManager *AccountManager::_instance = 0;

AccountManager *AccountManager::instance()
{
    static QMutex mutex;
    if (!_instance)
    {
        QMutexLocker lock(&mutex);
        if (!_instance) {
            _instance = new AccountManager;
        }
    }

    return _instance;
}

void AccountManager::setAccount(AccountPtr account)
{
    if (_account) {
        emit accountRemoved(_account);
    }
    _account = account;
    emit accountAdded(account);
}


Account::Account(QObject *parent)
    : QObject(parent)
    , _url(Theme::instance()->overrideServerUrl())
    , _am(0)
    , _credentials(0)
    , _treatSslErrorsAsFailure(false)
    , _davPath("/")
{
    qRegisterMetaType<AccountPtr>("AccountPtr");
}

AccountPtr Account::create()
{
    AccountPtr acc = AccountPtr(new Account);
    acc->setSharedThis(acc);
    return acc;
}

Account::~Account()
{
    delete _credentials;
    delete _am;
}

void Account::setSharedThis(AccountPtr sharedThis)
{
    _sharedThis = sharedThis.toWeakRef();
}

AccountPtr Account::sharedFromThis()
{
    return _sharedThis.toStrongRef();
}

void Account::save()
{
    QScopedPointer<QSettings> settings(settingsWithGroup(Theme::instance()->appName()));
    settings->setValue(QLatin1String(urlC), _url.toString());
    settings->setValue(QLatin1String(userC), _user);
    if (_credentials) {
        _credentials->persist();
        Q_FOREACH(QString key, _settingsMap.keys()) {
            settings->setValue(key, _settingsMap.value(key));
        }
        settings->setValue(QLatin1String(authTypeC), _credentials->authType());
    }
    settings->sync();

    // Save accepted certificates.
    settings->beginGroup(QLatin1String("General"));
    qDebug() << "Saving " << approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    Q_FOREACH( const QSslCertificate& cert, approvedCerts() ) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings->setValue( QLatin1String(caCertsKeyC), certs );
    }
}

AccountPtr Account::restore()
{
    // try to open the correctly themed settings
    QScopedPointer<QSettings> settings(settingsWithGroup(Theme::instance()->appName()));
    AccountPtr acc;
    QString dav_path("/");

    if (settings->childKeys().isEmpty())
        return AccountPtr();

    acc = AccountPtr(new Account);
    acc->setSharedThis(acc);

    acc->setUrl(settings->value(QLatin1String(urlC)).toUrl());
    acc->setUser(settings->value(QLatin1String(userC)).toString());
    acc->setCredentials(CredentialsFactory::create(settings->value(QLatin1String(authTypeC)).toString()));

    // We want to only restore settings for that auth type and the user value
    acc->_settingsMap.insert(QLatin1String(userC), settings->value(userC));

    dav_path.append(settings->value(userC).toString());
    acc->setDavPath(dav_path);

    QString authTypePrefix = settings->value(authTypeC).toString() + "_";
    Q_FOREACH(QString key, settings->childKeys()) {
        if (!key.startsWith(authTypePrefix))
            continue;
        acc->_settingsMap.insert(key, settings->value(key));
    }

    // now the cert, it is in the general group
    settings->beginGroup(QLatin1String("General"));
    acc->setApprovedCerts(QSslCertificate::fromData(settings->value(caCertsKeyC).toByteArray()));

    return acc;
}

static bool isEqualExceptProtocol(const QUrl &url1, const QUrl &url2)
{
    return (url1.host() != url2.host() ||
            url1.port() != url2.port() ||
            url1.path() != url2.path());
}

bool Account::changed(AccountPtr other, bool ignoreUrlProtocol) const
{
    if (!other) {
        return false;
    }
    bool changes = false;
    if (ignoreUrlProtocol) {
        changes = isEqualExceptProtocol(_url, other->_url);
    } else {
        changes = (_url == other->_url);
    }

    changes |= _credentials->changed(other->_credentials);

    return changes;
}

AbstractCredentials *Account::credentials() const
{
    return _credentials;
}

void Account::setCredentials(AbstractCredentials *cred)
{
    // set active credential manager
    QNetworkCookieJar *jar = 0;
    if (_am) {
        jar = _am->cookieJar();
        jar->setParent(0);

        _am->deleteLater();
    }

    if (_credentials) {
        credentials()->deleteLater();
    }
    cred->setAccount(this);
    _credentials = cred;
    _am = _credentials->getQNAM();
    if (jar) {
        _am->setCookieJar(jar);
    }
    connect(_am, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            SLOT(slotHandleErrors(QNetworkReply*,QList<QSslError>)));
    connect(_credentials, SIGNAL(fetched()),
            SLOT(slotCredentialsFetched()));
}

QUrl Account::davUrl() const
{
    return concatUrlPath(url(), davPath());
}

QList<QNetworkCookie> Account::lastAuthCookies() const
{
    return _am->cookieJar()->cookiesForUrl(_url);
}

void Account::clearCookieJar()
{
    _am->setCookieJar(new CookieJar);
}

QNetworkAccessManager *Account::networkAccessManager()
{
    return _am;
}

QNetworkReply *Account::headRequest(const QString &relPath)
{
    return headRequest(concatUrlPath(url(), relPath));
}

QNetworkReply *Account::headRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    return _am->head(request);
}

QNetworkReply *Account::getRequest(const QString &relPath)
{
    return getRequest(concatUrlPath(url(), relPath));
}

QNetworkReply *Account::getRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setSslConfiguration(this->createSslConfig());
    return _am->get(request);
}

QNetworkReply *Account::davRequest(const QByteArray &verb, const QString &relPath, QNetworkRequest req, QIODevice *data)
{
    return davRequest(verb, concatUrlPath(davUrl(), relPath), req, data);
}

QNetworkReply *Account::davRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    req.setUrl(url);
    req.setSslConfiguration(this->createSslConfig());
    return _am->sendCustomRequest(req, verb, data);
}

void Account::setCertificate(const QByteArray certficate, const QString privateKey)
{
    _pemCertificate=certficate;
    _pemPrivateKey=privateKey;
}

void Account::setSslConfiguration(const QSslConfiguration &config)
{
    _sslConfiguration = config;
}

QSslConfiguration Account::createSslConfig()
{
    // if setting the client certificate fails, you will probably get an error similar to this:
    //  "An internal error number 1060 happened. SSL handshake failed, client certificate was requested: SSL error: sslv3 alert handshake failure"
  
    // maybe this code must not have to be reevaluated every request?
    QSslConfiguration sslConfig;
    QSslCertificate sslClientCertificate;
    
    // maybe move this code from createSslConfig to the Account constructor
    ConfigFile cfgFile;
    if(!cfgFile.certificatePath().isEmpty() && !cfgFile.certificatePasswd().isEmpty()) {
        resultP12ToPem certif = p12ToPem(cfgFile.certificatePath().toStdString(), cfgFile.certificatePasswd().toStdString());
        QString s = QString::fromStdString(certif.Certificate);
        QByteArray ba = s.toLocal8Bit();
        this->setCertificate(ba, QString::fromStdString(certif.PrivateKey));
    }
    if((!_pemCertificate.isEmpty())&&(!_pemPrivateKey.isEmpty())) {
        // Read certificates
        QList<QSslCertificate> sslCertificateList = QSslCertificate::fromData(_pemCertificate, QSsl::Pem);
        if(sslCertificateList.length() != 0) {
            sslClientCertificate = sslCertificateList.takeAt(0);
        }
        // Read key from file
        QSslKey privateKey(_pemPrivateKey.toLocal8Bit(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey , "");

        // SSL configuration
        sslConfig.defaultConfiguration();
        sslConfig.setCaCertificates(QSslSocket::systemCaCertificates());
        QList<QSslCertificate> caCertifs = sslConfig.caCertificates();
        sslConfig.setLocalCertificate(sslClientCertificate);
        sslConfig.setPrivateKey(privateKey);
        qDebug() << "Added SSL client certificate to the query";
    }

    return sslConfig;
}

void Account::setApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts = certs;
}

void Account::addApprovedCerts(const QList<QSslCertificate> certs)
{
    _approvedCerts += certs;
}

void Account::setSslErrorHandler(AbstractSslErrorHandler *handler)
{
    _sslErrorHandler.reset(handler);
}

void Account::setUrl(const QUrl &url)
{
    _url = url;
}

void Account::setUser(const QString &user)
{
    _user = user;
}

QUrl Account::concatUrlPath(const QUrl &url, const QString &concatPath,
                            const QList< QPair<QString, QString> > &queryItems)
{
    QString path = url.path();
    if (! concatPath.isEmpty()) {
        // avoid '//'
        if (path.endsWith('/') && concatPath.startsWith('/')) {
            path.chop(1);
        } // avoid missing '/'
        else if (!path.endsWith('/') && !concatPath.startsWith('/')) {
            path += QLatin1Char('/');
        }
        path += concatPath; // put the complete path together
    }

    QUrl tmpUrl = url;
    tmpUrl.setPath(path);
    if( queryItems.size() > 0 ) {
        tmpUrl.setQueryItems(queryItems);
    }
    return tmpUrl;
}

QString Account::_configFileName;

QSettings *Account::settingsWithGroup(const QString& group, QObject *parent)
{
    if (_configFileName.isEmpty()) {
        // cache file name
        ConfigFile cfg;
        _configFileName = cfg.configFile();
    }
    QSettings *settings = new QSettings(_configFileName, QSettings::IniFormat, parent);
    settings->beginGroup(group);
    return settings;
}

QVariant Account::credentialSetting(const QString &key) const
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        QString value = _settingsMap.value(prefix+"_"+key).toString();
        if (value.isEmpty()) {
            value = _settingsMap.value(key).toString();
        }
        return value;
    }
    return QVariant();
}

void Account::setCredentialSetting(const QString &key, const QVariant &value)
{
    if (_credentials) {
        QString prefix = _credentials->authType();
        _settingsMap.insert(prefix+"_"+key, value);
    }
}

void Account::slotHandleErrors(QNetworkReply *reply , QList<QSslError> errors)
{
    NetworkJobTimeoutPauser pauser(reply);
    QString out;
    QDebug(&out) << "SSL-Errors happened for url " << reply->url().toString();
    foreach(const QSslError &error, errors) {
        QDebug(&out) << "\tError in " << error.certificate() << ":"
                     << error.errorString() << "("<< error.error() << ")" << "\n";
    }

    if( _treatSslErrorsAsFailure ) {
        // User decided once not to trust. Honor this decision.
        qDebug() << out << "Certs not trusted by user decision, returning.";
        return;
    }

    QList<QSslCertificate> approvedCerts;
    if (_sslErrorHandler.isNull() ) {
        qDebug() << out << Q_FUNC_INFO << "called without valid SSL error handler for account" << url();
        return;
    }

    if (_sslErrorHandler->handleErrors(errors, &approvedCerts, sharedFromThis())) {
        QSslSocket::addDefaultCaCertificates(approvedCerts);
        addApprovedCerts(approvedCerts);
        // all ssl certs are known and accepted. We can ignore the problems right away.
//         qDebug() << out << "Certs are known and trusted! This is not an actual error.";
        reply->ignoreSslErrors();
    } else {
        _treatSslErrorsAsFailure = true;
        return;
    }
}

void Account::slotCredentialsFetched()
{
    emit credentialsFetched(_credentials);
}

void Account::handleInvalidCredentials()
{
    // invalidate & forget token/password
    // but try to re-sign in.
    if (_credentials->ready()) {
        _credentials->invalidateAndFetch();
    } else {
        _credentials->fetch();
    }

    emit invalidCredentials();
}

} // namespace OCC
