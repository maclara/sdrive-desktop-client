#include "setupwizardcontroller.h"

#include "accessmanager.h"
#include "determineauthtypejobfactory.h"
#include "gui/application.h"
#include "gui/folderman.h"
#include "pages/accountconfiguredwizardpage.h"
#include "pages/basiccredentialssetupwizardpage.h"
#include "states/abstractsetupwizardstate.h"
#include "states/accountconfiguredsetupwizardstate.h"
#include "states/basiccredentialssetupwizardstate.h"
#include "states/oauthcredentialssetupwizardstate.h"
#include "states/serverurlsetupwizardstate.h"
#include "states/webfingersetupwizardstate.h"

#include <QClipboard>
#include <QTimer>

using namespace std::chrono_literals;

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardController, "setupwizard.controller")

SetupWizardController::SetupWizardController(QWidget *parent)
    : QObject(parent)
    , _context(new SetupWizardContext(parent, this))
{
    // initialize pagination
    _context->window()->setNavigationEntries({
        SetupWizardState::ServerUrlState,
        SetupWizardState::WebFingerState,
        SetupWizardState::CredentialsState,
        SetupWizardState::AccountConfiguredState,
    });

    // set up initial state
    changeStateTo(SetupWizardState::FirstState);

    // allow settings dialog to clean up the wizard controller and all the objects it created
    connect(_context->window(), &SetupWizardWindow::rejected, this, [this]() {
        qCDebug(lcSetupWizardController) << "wizard window closed";
        Q_EMIT finished(nullptr, SyncMode::Invalid);
    });

    connect(_context->window(), &SetupWizardWindow::paginationEntryClicked, this, [this](SetupWizardState clickedState) {
        qCDebug(lcSetupWizardController) << "pagination entry clicked: current state" << _currentState << "clicked state" << clickedState;
        changeStateTo(clickedState);
    });

    connect(_context->window(), &SetupWizardWindow::nextButtonClicked, this, [this]() {
        qCDebug(lcSetupWizardController) << "next button clicked, current state" << _currentState;
        _currentState->evaluatePage();
    });

    // in case the back button is clicked, the current page's data is dismissed, and the previous page should be shown
    connect(_context->window(), &SetupWizardWindow::backButtonClicked, this, [this]() {
        // with enum classes, we have to explicitly cast to a numeric value
        const auto currentStateIdx = static_cast<int>(_currentState->state());
        Q_ASSERT(currentStateIdx > 0);

        qCDebug(lcSetupWizardController) << "back button clicked, current state" << _currentState;

        const auto previousState = static_cast<SetupWizardState>(currentStateIdx - 1);
        changeStateTo(previousState);
    });
}

SetupWizardWindow *SetupWizardController::window()
{
    return _context->window();
}

void SetupWizardController::changeStateTo(SetupWizardState nextState)
{
    // should take care of cleaning up the page once the function has finished
    QScopedPointer<AbstractSetupWizardState> page(_currentState);

    // validate initial state
    Q_ASSERT(nextState == SetupWizardState::FirstState || _currentState != nullptr);

    if (_currentState != nullptr) {
        _currentState->deleteLater();
    }

    switch (nextState) {
    case SetupWizardState::ServerUrlState: {
        _currentState = new ServerUrlSetupWizardState(_context);
        break;
    }
    case SetupWizardState::WebFingerState: {
        _currentState = new WebFingerSetupWizardState(_context);
        break;
    }
    case SetupWizardState::CredentialsState: {
        switch (_context->accountBuilder().authType()) {
        case DetermineAuthTypeJob::AuthType::Basic:
            _currentState = new BasicCredentialsSetupWizardState(_context);
            break;
        case DetermineAuthTypeJob::AuthType::OAuth:
            _currentState = new OAuthCredentialsSetupWizardState(_context);
            break;
        default:
            Q_UNREACHABLE();
        }

        break;
    }
    case SetupWizardState::AccountConfiguredState: {
        _currentState = new AccountConfiguredSetupWizardState(_context);
        break;
    }
    default:
        Q_UNREACHABLE();
    }

    Q_ASSERT(_currentState != nullptr);
    Q_ASSERT(_currentState->state() == nextState);

    qCDebug(lcSetupWizardController) << "Current wizard state:" << _currentState->state();

    connect(_currentState, &AbstractSetupWizardState::evaluationSuccessful, this, [this]() {
        _currentState->deleteLater();

        switch (_currentState->state()) {
        case SetupWizardState::ServerUrlState: {
            changeStateTo(SetupWizardState::WebFingerState);
            return;
            Q_FALLTHROUGH();
        }
        case SetupWizardState::WebFingerState: {
            changeStateTo(SetupWizardState::CredentialsState);
            return;
        }
        case SetupWizardState::CredentialsState: {
            changeStateTo(SetupWizardState::AccountConfiguredState);
            return;
        }
        case SetupWizardState::AccountConfiguredState: {
            const auto *pagePtr = qobject_cast<AccountConfiguredWizardPage *>(_currentState->page());

            auto account = _context->accountBuilder().build();
            Q_ASSERT(account != nullptr);
            Q_EMIT finished(account, pagePtr->syncMode());
            return;
        }
        default:
            Q_UNREACHABLE();
        }
    });

    connect(_currentState, &AbstractSetupWizardState::evaluationFailed, this, [this](const QString &errorMessage) {
        _currentState->deleteLater();
        _context->window()->showErrorMessage(errorMessage);
        changeStateTo(_currentState->state());
    });

    _context->window()->displayPage(_currentState->page(), _currentState->state());
}

SetupWizardController::~SetupWizardController() noexcept
{
    _context->deleteLater();
}
}
