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

#include <openssl/crypto.h>

#include "owncloudtheme.h"

#include <QString>
#include <QVariant>
#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#endif
#include <QCoreApplication>
#include <QDebug>

#include "version.h"
#include "config.h"

namespace OCC {

class SwissDiskTheme : public Theme
{
public:
    SwissDiskTheme() : Theme() { /* qDebug() << " ** running ownCloud theme!"; */ }

    QString configFileName() const Q_DECL_OVERRIDE
    {
	return QLatin1String("swissdisk.cfg");
    }

    QString about() const Q_DECL_OVERRIDE
    {
	QString devString;
	devString = trUtf8("<p>Version %2. For more information visit <a href=\"%3\">%4</a></p>"
               "<p><small>By Klaas Freitag, Daniel Molkentin, Jan-Christoph Borchardt, "
               "Olivier Goffart, Markus Götz and others.</small></p>"
               "<p>Copyright ownCloud, Inc.</p>"
               "<p>Licensed under the GNU General Public License (GPL) Version 2.0<br/>"
               "ownCloud and the ownCloud Logo are registered trademarks of ownCloud, "
               "Inc. in the United States, other countries, or both.</p>"
               "<p>All SwissDisk logos and the SwissDisk name are Copyright "
               "<a href='https://maclara-llc.com/'>maClara, LLC.</a></p>"
               )
            .arg(MIRALL_VERSION_STRING)
            .arg("http://" MIRALL_STRINGIFY(APPLICATION_DOMAIN))
            .arg(MIRALL_STRINGIFY(APPLICATION_DOMAIN));

	devString += gitSHA1();
	return devString;
    }

#ifndef TOKEN_AUTH_ONLY
    QIcon applicationIcon( ) const Q_DECL_OVERRIDE
    {
	return themeIcon( QLatin1String("swissdisk-icon") );
    }
#endif

#ifndef TOKEN_AUTH_ONLY
    QColor wizardHeaderBackgroundColor() const Q_DECL_OVERRIDE
    {
        return QColor("#1d2d42");
    }

    QColor wizardHeaderTitleColor() const Q_DECL_OVERRIDE
    {
        return QColor("#ffffff");
    }
#endif

    QString appName() const Q_DECL_OVERRIDE
    {
        return QLatin1String("SwissDisk");
    }

    QString appNameGUI() const Q_DECL_OVERRIDE
    {
        return QLatin1String("SwissDisk");
    }

    QString overrideServerUrl() const Q_DECL_OVERRIDE
    {
        return QLatin1String("https://disk.swissdisk.com");
    }

    virtual QString gitSHA1() const
    {
        QString devString;
#ifdef GIT_SHA1
        const QString githubPrefix(QLatin1String(
                                       "https://github.com/maclara/sdrive-desktop-client/commit/"));
        const QString gitSha1(QLatin1String(GIT_SHA1));
        devString = QCoreApplication::translate("about()",
                       "<p><small>Built from Git revision <a href=\"%1\">%2</a>"
                       " on %3, %4 using Qt %5, %6</small></p>")
                .arg(githubPrefix+gitSha1).arg(gitSha1.left(6))
                .arg(__DATE__).arg(__TIME__)
                .arg(QT_VERSION_STR)
                .arg(QString::fromAscii(OPENSSL_VERSION_TEXT));
#endif
        return devString;
    }

    virtual QString updateCheckUrl() const {
        return QLatin1String( "https://www.swissdisk.com/client/" );
    }

    QString helpUrl() const
    {
        return QString::fromLatin1("https://www.swissdisk.com/client/%1.%2/").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR);
    }
private:
};

} // namespace OCC
