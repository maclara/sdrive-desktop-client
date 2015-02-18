/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef CONNECTIONVALIDATOR_H
#define CONNECTIONVALIDATOR_H

#include "owncloudlib.h"
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QNetworkReply>
#include "accountfwd.h"

namespace OCC {

/**
 * This is a job-like class to check that the server is up and that we are connected.
 * There is two entry point: checkServerAndAuth and checkAuthentication
 * checkAutentication is the quick version that only does the propfind
 * while checkServerAndAuth is doing the 2 calls.
 *
 * Here follows the state machine


*---> checkServerAndAuth  (check status.php)
        CheckServerJob
        |
        +-> slotNoStatusFound --> X
        |
        +-> slotJobTimeout --> X
        |
        +-> slotStatusFound
                credential->fetch() --+
                                      |
  +-----------------------------------+
  |
*-+-> checkAuthentication (PROPFIND on root)
        PropfindJob
        |
        +-> slotAuthFailed --> X
        |
        +-> slotAuthSuccess --> X
 */
class OWNCLOUDSYNC_EXPORT ConnectionValidator : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionValidator(AccountPtr account, QObject *parent = 0);

    enum Status {
        Undefined,
        Connected,
        NotConfigured,
        ServerVersionMismatch,
        CredentialsWrong,
        StatusNotFound,
        UserCanceledCredentials,
        // actually also used for other errors on the authed request
        Timeout
    };

    static QString statusString( Status );

public slots:
    /// Checks the server and the authentication.
    void checkServerAndAuth();

    /// Checks authentication only.
    void checkAuthentication();

signals:
    void connectionResult( ConnectionValidator::Status status, QStringList errors );

protected slots:
    void slotStatusFound(const QUrl&url);
    void slotNoStatusFound(QNetworkReply *reply);
    void slotJobTimeout(const QUrl& url);

    void slotAuthFailed(QNetworkReply *reply);
    void slotAuthSuccess();

private:
    void reportResult(Status status);

    QStringList _errors;
    AccountPtr   _account;
};

}

#endif // CONNECTIONVALIDATOR_H
