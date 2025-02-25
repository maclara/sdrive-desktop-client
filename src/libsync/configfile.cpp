/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "config.h"

#include "configfile.h"
#include "theme.h"
#include "utility.h"

#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"

#ifndef TOKEN_AUTH_ONLY
#include <QWidget>
#include <QHeaderView>
#include <QDesktopServices>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>
#include <QNetworkProxy>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000 // default remote poll time in milliseconds
#define DEFAULT_MAX_LOG_LINES 20000

namespace OCC {

static const char caCertsKeyC[] = "CaCertificates";
static const char remotePollIntervalC[] = "remotePollInterval";
static const char forceSyncIntervalC[] = "forceSyncInterval";
static const char monoIconsC[] = "monoIcons";
static const char crashReporterC[] = "crashReporter";
static const char optionalDesktopNoficationsC[] = "optionalDesktopNotifications";
static const char skipUpdateCheckC[] = "skipUpdateCheck";
static const char geometryC[] = "geometry";
static const char timeoutC[] = "timeout";

static const char proxyHostC[] = "Proxy/host";
static const char proxyTypeC[] = "Proxy/type";
static const char proxyPortC[] = "Proxy/port";
static const char proxyUserC[] = "Proxy/user";
static const char proxyPassC[] = "Proxy/pass";
static const char proxyNeedsAuthC[] = "Proxy/needsAuth";

static const char useUploadLimitC[]   = "BWLimit/useUploadLimit";
static const char useDownloadLimitC[] = "BWLimit/useDownloadLimit";
static const char uploadLimitC[]      = "BWLimit/uploadLimit";
static const char downloadLimitC[]    = "BWLimit/downloadLimit";

static const char maxLogLinesC[] = "Logging/maxLogLines";

const char certPath[] = "http_certificatePath";
const char certPasswd[] = "http_certificatePasswd";
QString ConfigFile::_confDir = QString();
bool    ConfigFile::_askedUser = false;

ConfigFile::ConfigFile()
{
    // QDesktopServices uses the application name to create a config path
    qApp->setApplicationName( Theme::instance()->appNameGUI() );

    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString config = configFile();


    QSettings settings(config, QSettings::IniFormat);
    settings.beginGroup( defaultConnection() );

    // qDebug() << Q_FUNC_INFO << "Loading config: " << config << " (URL is " << settings.value("url").toString() << ")";
}

void ConfigFile::setConfDir(const QString &value)
{
    QString dirPath = value;
    if( dirPath.isEmpty() ) return;

    QFileInfo fi(dirPath);
    if ( !fi.exists() && !fi.isAbsolute() ) {
        QDir::current().mkdir(dirPath);
        QDir dir = QDir::current();
        dir.cd(dirPath);
        fi.setFile(dir.path());
    }
    if( fi.exists() && fi.isDir() ) {
        dirPath = fi.absoluteFilePath();
        qDebug() << "** Using custom config dir " << dirPath;
        _confDir=dirPath;
    }
}

bool ConfigFile::optionalDesktopNotifications() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(optionalDesktopNoficationsC), true).toBool();
}

int ConfigFile::timeout() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(timeoutC), 300).toInt(); // default to 5 min
}

void ConfigFile::setOptionalDesktopNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(optionalDesktopNoficationsC), show);
    settings.sync();
}

void ConfigFile::saveGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    Q_ASSERT(!w->objectName().isNull());
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(w->objectName());
    settings.setValue(QLatin1String(geometryC), w->saveGeometry());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    w->restoreGeometry(getValue(geometryC, w->objectName()).toByteArray());
#endif
}

void ConfigFile::saveGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if(!header) return;
    Q_ASSERT(!header->objectName().isNull());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    settings.setValue(QLatin1String(geometryC), header->saveState());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if(!header) return;
    Q_ASSERT(!header->objectName().isNull());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    header->restoreState(getValue(geometryC, header->objectName()).toByteArray());
#endif
}

QVariant ConfigFile::getPolicySetting(const QString &setting, const QVariant& defaultValue) const
{
    if (Utility::isWindows()) {
        // check for policies first and return immediately if a value is found.
        QSettings userPolicy(QString::fromLatin1("HKEY_CURRENT_USER\\Software\\Policies\\%1\\%2")
                             .arg(APPLICATION_VENDOR).arg(Theme::instance()->appName()),
                             QSettings::NativeFormat);
        if(userPolicy.contains(setting)) {
            return userPolicy.value(setting);
        }

        QSettings machinePolicy(QString::fromLatin1("HKEY_LOCAL_MACHINE\\Software\\Policies\\%1\\%2")
                                .arg(APPLICATION_VENDOR).arg(APPLICATION_NAME),
                                QSettings::NativeFormat);
        if(machinePolicy.contains(setting)) {
            return machinePolicy.value(setting);
        }
    }
    return defaultValue;
}

QString ConfigFile::configPath() const
{
    #ifndef TOKEN_AUTH_ONLY
    if( _confDir.isEmpty() ) {
        //  Qt 5's QStandardPaths::writableLocation gives us wrong results (without /data/),
        //  so we'll have to use the deprecated version for now
        _confDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    }
    #endif
    QString dir = _confDir;

    if( !dir.endsWith(QLatin1Char('/')) ) dir.append(QLatin1Char('/'));
    return dir;
}

QString ConfigFile::configPathWithAppName() const
{
    //HACK
    return QFileInfo( configFile() ).dir().absolutePath().append("/");
}

static const QLatin1String exclFile("sync-exclude.lst");

QString ConfigFile::excludeFile(Scope scope) const
{
    // prefer sync-exclude.lst, but if it does not exist, check for
    // exclude.lst for compatibility reasons in the user writeable
    // directories.
    QFileInfo fi;

    if (scope != SystemScope) {
        QFileInfo fi;
        fi.setFile( configPath(), exclFile );

        if( ! fi.isReadable() ) {
            fi.setFile( configPath(), QLatin1String("exclude.lst") );
        }
        if( ! fi.isReadable() ) {
            fi.setFile( configPath(), exclFile );
        }
        return fi.absoluteFilePath();
    } else if (scope != UserScope) {
        return ConfigFile::excludeFileFromSystem();
    } else {
        Q_ASSERT(false);
        return QString(); // unreachable
    }
}

QString ConfigFile::excludeFileFromSystem()
{
    QFileInfo fi;
#ifdef Q_OS_WIN
    fi.setFile( QCoreApplication::applicationDirPath(), exclFile );
#endif
#ifdef Q_OS_UNIX
    fi.setFile( QString( SYSCONFDIR "/%1").arg(Theme::instance()->appName()), exclFile );
    if ( ! fi.exists() ) {
        // Prefer to return the preferred path! Only use the fallback location
        // if the other path does not exist and the fallback is valid.
        QFileInfo nextToBinary( QCoreApplication::applicationDirPath(), exclFile );
        if (nextToBinary.exists()) {
            fi = nextToBinary;
        }
    }
#endif
#ifdef Q_OS_MAC
    // exec path is inside the bundle
    fi.setFile( QCoreApplication::applicationDirPath(),
                QLatin1String("../Resources/") + exclFile );
#endif
    
    return fi.absoluteFilePath();
}

QString ConfigFile::configFile() const
{
    return configPath() + Theme::instance()->configFileName();
}

bool ConfigFile::exists()
{
    QFile file( configFile() );
    return file.exists();
}

QString ConfigFile::defaultConnection() const
{
    return Theme::instance()->appName();
}

void ConfigFile::storeData(const QString& group, const QString& key, const QVariant& value)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.setValue(key, value);
    settings.sync();
}

QVariant ConfigFile::retrieveData(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.value(key);
}

void ConfigFile::removeData(const QString& group, const QString& key)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.remove(key);
}

bool ConfigFile::dataExists(const QString& group, const QString& key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.contains(key);
}

QByteArray ConfigFile::caCerts( )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value( QLatin1String(caCertsKeyC) ).toByteArray();
}

void ConfigFile::setCaCerts( const QByteArray & certs )
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue( QLatin1String(caCertsKeyC), certs );
    settings.sync();
}

int ConfigFile::remotePollInterval( const QString& connection ) const
{
  QString con( connection );
  if( connection.isEmpty() ) con = defaultConnection();

  QSettings settings(configFile(), QSettings::IniFormat);
  settings.beginGroup( con );

  int remoteInterval = settings.value( QLatin1String(remotePollIntervalC), DEFAULT_REMOTE_POLL_INTERVAL ).toInt();
  if( remoteInterval < 5000) {
    qDebug() << "Remote Interval is less than 5 seconds, reverting to" << DEFAULT_REMOTE_POLL_INTERVAL;
    remoteInterval = DEFAULT_REMOTE_POLL_INTERVAL;
  }
  return remoteInterval;
}

void ConfigFile::setRemotePollInterval(int interval, const QString &connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    if( interval < 5000 ) {
        qDebug() << "Remote Poll interval of " << interval << " is below fife seconds.";
        return;
    }
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup( con );
    settings.setValue(QLatin1String(remotePollIntervalC), interval );
    settings.sync();
}

quint64 ConfigFile::forceSyncInterval(const QString& connection) const
{
    uint pollInterval = remotePollInterval(connection);

    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup( con );

    quint64 defaultInterval = 2 * 60 * 60 * 1000ull; // 2h
    quint64 interval = settings.value( QLatin1String(forceSyncIntervalC), defaultInterval ).toULongLong();
    if( interval < pollInterval) {
        qDebug() << "Force sync interval is less than the remote poll inteval, reverting to" << pollInterval;
        interval = pollInterval;
    }
    return interval;
}

bool ConfigFile::skipUpdateCheck( const QString& connection ) const
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QVariant fallback = getValue(QLatin1String(skipUpdateCheckC), con, false);
    fallback = getValue(QLatin1String(skipUpdateCheckC), QString(), fallback);

    QVariant value = getPolicySetting(QLatin1String(skipUpdateCheckC), fallback);
    return value.toBool();
}

void ConfigFile::setSkipUpdateCheck( bool skip, const QString& connection )
{
    QString con( connection );
    if( connection.isEmpty() ) con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup( con );

    settings.setValue( QLatin1String(skipUpdateCheckC), QVariant(skip) );
    settings.sync();

}

int ConfigFile::maxLogLines() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value( QLatin1String(maxLogLinesC), DEFAULT_MAX_LOG_LINES ).toInt();
}

void ConfigFile::setMaxLogLines( int lines )
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(maxLogLinesC), lines);
    settings.sync();
}

void ConfigFile::setProxyType(int proxyType,
                  const QString& host,
                  int port, bool needsAuth,
                  const QString& user,
                  const QString& pass)
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue(QLatin1String(proxyTypeC), proxyType);

    if (proxyType == QNetworkProxy::HttpProxy ||
        proxyType == QNetworkProxy::Socks5Proxy) {
        settings.setValue(QLatin1String(proxyHostC), host);
        settings.setValue(QLatin1String(proxyPortC), port);
        settings.setValue(QLatin1String(proxyNeedsAuthC), needsAuth);
        settings.setValue(QLatin1String(proxyUserC), user);
        settings.setValue(QLatin1String(proxyPassC), pass.toUtf8().toBase64());
    }
    settings.sync();
}

QVariant ConfigFile::getValue(const QString& param, const QString& group,
                                    const QVariant& defaultValue) const
{
    QVariant systemSetting;
    if (Utility::isMac()) {
            QSettings systemSettings(QLatin1String("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"), QSettings::NativeFormat);
            if (!group.isEmpty()) {
                systemSettings.beginGroup(group);
            }
            systemSetting = systemSettings.value(param, defaultValue);
    } else if (Utility::isUnix()) {
        QSettings systemSettings(QString( SYSCONFDIR "/%1/%1.conf").arg(Theme::instance()->appName()), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else { // Windows
        QSettings systemSettings(QString::fromLatin1("HKEY_LOCAL_MACHINE\\Software\\%1\\%2")
                                .arg(APPLICATION_VENDOR).arg(Theme::instance()->appName()),
                                QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    }

    QSettings settings(configFile(), QSettings::IniFormat);
    if (!group.isEmpty()) settings.beginGroup(group);

    return settings.value(param, systemSetting);
}

void ConfigFile::setValue(const QString& key, const QVariant &value)
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue(key, value);
}

int ConfigFile::proxyType() const
{
    return getValue(QLatin1String(proxyTypeC)).toInt();
}

QString ConfigFile::proxyHostName() const
{
    return getValue(QLatin1String(proxyHostC)).toString();
}

int ConfigFile::proxyPort() const
{
    return getValue(QLatin1String(proxyPortC)).toInt();
}

bool ConfigFile::proxyNeedsAuth() const
{
    return getValue(QLatin1String(proxyNeedsAuthC)).toBool();
}

QString ConfigFile::proxyUser() const
{
    return getValue(QLatin1String(proxyUserC)).toString();
}

QString ConfigFile::proxyPassword() const
{
    QByteArray pass = getValue(proxyPassC).toByteArray();
    return QString::fromUtf8(QByteArray::fromBase64(pass));
}

int ConfigFile::useUploadLimit() const
{
    return getValue(useUploadLimitC, QString(), 0).toInt();
}

bool ConfigFile::useDownloadLimit() const
{
    return getValue(useDownloadLimitC, QString(), false).toBool();
}

void ConfigFile::setUseUploadLimit(int val)
{
    setValue(useUploadLimitC, val);
}

void ConfigFile::setUseDownloadLimit(bool enable)
{
    setValue(useDownloadLimitC, enable);
}

int ConfigFile::uploadLimit() const
{
    return getValue(uploadLimitC, QString(), 10).toInt();
}

int ConfigFile::downloadLimit() const
{
    return getValue(downloadLimitC, QString(), 80).toInt();
}

void ConfigFile::setUploadLimit(int kbytes)
{
    setValue(uploadLimitC, kbytes);
}

void ConfigFile::setDownloadLimit(int kbytes)
{
    setValue(downloadLimitC, kbytes);
}

bool ConfigFile::monoIcons() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(monoIconsC), false).toBool();
}

void ConfigFile::setMonoIcons(bool useMonoIcons)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(monoIconsC), useMonoIcons);
}

bool ConfigFile::crashReporter() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(crashReporterC), true).toBool();
}

void ConfigFile::setCrashReporter(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(crashReporterC), enabled);
}

QString ConfigFile::certificatePath() const
{
    return retrieveData(QString(), QLatin1String(certPath)).toString();
}

void ConfigFile::setCertificatePath(const QString& cPath)
{
     QSettings settings(configFile(), QSettings::IniFormat);
     settings.setValue( QLatin1String(certPath), cPath);
     settings.sync();
}

QString ConfigFile::certificatePasswd() const
{
    return retrieveData(QString(), QLatin1String(certPasswd)).toString();
}

void ConfigFile::setCertificatePasswd(const QString& cPasswd)
{
     QSettings settings(configFile(), QSettings::IniFormat);
     settings.setValue( QLatin1String(certPasswd), cPasswd);
     settings.sync();
}

}
