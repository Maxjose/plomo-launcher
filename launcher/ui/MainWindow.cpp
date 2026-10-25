// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "PlomoPackUpdater.h"
#include "PlomoSelfUpdater.h"
#include "ui/PlomoPages.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/MinecraftLoadAndCheck.h"
#include "minecraft/launch/AutoInstallJava.h"
#include "net/Mode.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QShortcut>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStackedWidget>
#include <QVersionNumber>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <net/ApiDownload.h>
#include <net/NetJob.h>
#include <news/NewsChecker.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "ui/widgets/LabeledToolButton.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"

#include "KonamiCode.h"

#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // instance toolbar stuff
    {
        // Qt doesn't like vertical moving toolbars, so we have to force them...
        // See https://github.com/PolyMC/PolyMC/issues/493
        connect(ui->instanceToolBar, &QToolBar::orientationChanged,
                [this](Qt::Orientation) { ui->instanceToolBar->setOrientation(Qt::Vertical); });

        // if you try to add a widget to a toolbar in a .ui file
        // qt designer will delete it when you save the file >:(
        changeIconButton = new LabeledToolButton(this);
        changeIconButton->setObjectName(QStringLiteral("changeIconButton"));
        changeIconButton->setIcon(QIcon::fromTheme("news"));
        changeIconButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(changeIconButton, &QToolButton::clicked, this, &MainWindow::on_actionChangeInstIcon_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, changeIconButton);

        renameButton = new LabeledToolButton(this);
        renameButton->setObjectName(QStringLiteral("renameButton"));
        renameButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(renameButton, &QToolButton::clicked, this, &MainWindow::on_actionRenameInstance_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, renameButton);

        ui->instanceToolBar->insertSeparator(ui->actionLaunchInstance);

        // restore the instance toolbar settings
        const auto setting_name = QString("WideBarVisibility_%1").arg(ui->instanceToolBar->objectName());
        instanceToolbarSetting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

        ui->instanceToolBar->setVisibilityState(QByteArray::fromBase64(instanceToolbarSetting->get().toString().toUtf8()));

        ui->instanceToolBar->addContextMenuAction(ui->newsToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->instanceToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->actionToggleStatusBar);
        ui->instanceToolBar->addContextMenuAction(ui->actionLockToolbars);
    }

    // set the menu for the folders help, accounts, and export tool buttons
    {
        auto foldersMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionFoldersButton));
        ui->actionFoldersButton->setMenu(ui->foldersMenu);
        foldersMenuButton->setPopupMode(QToolButton::InstantPopup);

        helpMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionHelpButton));
        ui->actionHelpButton->setMenu(new QMenu(this));
        ui->actionHelpButton->menu()->addActions(ui->helpMenu->actions());
        ui->actionHelpButton->menu()->removeAction(ui->actionCheckUpdate);
        helpMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto accountMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionAccountsButton));
        accountMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        ui->instanceToolBar->setEnabled(false);
        setInstanceActionsEnabled(false);

        // add a close button at the end of the main toolbar when running on gamescope / steam deck
        // this is only needed on gamescope because it defaults to an X11/XWayland session and
        // does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            ui->mainToolBar->addAction(ui->actionCloseWindow);
        }

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // add the toolbar toggles to the view menu
    ui->viewMenu->addAction(ui->instanceToolBar->toggleViewAction());
    ui->viewMenu->addAction(ui->newsToolBar->toggleViewAction());

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);
    }

    // Konami Code
    {
        secretEventFilter = new KonamiCode(this);
        connect(secretEventFilter, &KonamiCode::triggered, this, &MainWindow::konamiTriggered);
    }

    // Add the news label to the news toolbar.
    {
        m_newsChecker.reset(new NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL));
        newsLabel = new QToolButton();
        newsLabel->setIcon(QIcon::fromTheme("news"));
        newsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        newsLabel->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        newsLabel->setFocusPolicy(Qt::NoFocus);
        ui->newsToolBar->insertWidget(ui->actionMoreNews, newsLabel);

        connect(newsLabel, &QAbstractButton::clicked, this, &MainWindow::newsButtonClicked);
        connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, &MainWindow::updateNewsLabel);
        connect(APPLICATION, &Application::shouldReloadNews, m_newsChecker.get(), &NewsChecker::reloadNews);
        updateNewsLabel();
    }

    // Create the instance list widget
    {
        view = new InstanceView(ui->centralWidget);

        view->setSelectionMode(QAbstractItemView::SingleSelection);
        // FIXME: leaks ListViewDelegate
        auto delegate = new ListViewDelegate(this);
        view->setItemDelegate(delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(delegate, &ListViewDelegate::textChanged, this, [this](QString before, QString after) {
            if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
                auto oldID = m_selectedInstance->id();
                auto newID = QFileInfo(newRoot).fileName();
                QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
                bool syncGroup = origGroup != GroupId() && oldID != newID;
                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

                refreshInstances();
                setSelectedInstanceById(newID);

                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(newID, origGroup);
            }
        });

        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);
        ui->horizontalLayout->addWidget(view);
    }
    // The cat background
    {
        // set the cat action priority here so you can still see the action in qt designer
        ui->actionCAT->setPriority(QAction::LowPriority);
        bool cat_enable = APPLICATION->settings()->get("TheCat").toBool();
        ui->actionCAT->setChecked(cat_enable);
        connect(ui->actionCAT, &QAction::toggled, this, &MainWindow::onCatToggled);
        connect(APPLICATION, &Application::currentCatChanged, this, &MainWindow::onCatChanged);
        setCatBackground(cat_enable);
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

    // Lock toolbars
    {
        bool toolbarsLocked = APPLICATION->settings()->get("ToolbarsLocked").toBool();
        ui->actionLockToolbars->setChecked(toolbarsLocked);
        connect(ui->actionLockToolbars, &QAction::toggled, this, &MainWindow::lockToolbars);
        lockToolbars(toolbarsLocked);
    }
    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance toolbar
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the toolbar!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Add "manage accounts" button, right align
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listActivityChanged, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // load the news
    {
        //m_newsChecker->reloadNews();
        updateNewsLabel();
    }

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    setupPlomoUi();

    // removing this looks stupid
    view->setFocus();

    retranslateUi();
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
        QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());
    renameButton->setToolTip(ui->actionRenameInstance->toolTip());

    // replace the %1 with the launcher display name in some actions
    if (helpMenuButton->toolTip().contains("%1"))
        helpMenuButton->setToolTip(helpMenuButton->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));

    for (auto action : ui->helpMenu->actions()) {
        if (action->text().contains("%1"))
            action->setText(action->text().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        if (action->toolTip().contains("%1"))
            action->setToolTip(action->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}

MainWindow::~MainWindow() {}

namespace {
constexpr auto kLauncherInfoUrl = "https://cdn.playplomo.com/launcher/launcher.json";
constexpr auto kDownloadPageFallback = "https://playplomo.com/launcher/";
}  // namespace

void MainWindow::setupPlomoUi()
{
    plomoInstallLanguage(APPLICATION->settings()->get("Language").toString());
    auto instanceDir = APPLICATION->settings()->get("InstanceDir").toString();
    if (QDir(instanceDir).isRelative())
        instanceDir = QDir(APPLICATION->dataRoot()).filePath(instanceDir);
    m_plomoRoot = QDir(instanceDir).filePath("PLOMO");
    m_plomoUpdater = new PlomoPackUpdater(m_plomoRoot, this);
    m_plomoUpdater->setAlphaChannel(APPLICATION->settings()->get("PlomoAlphaChannel").toBool());

    setWindowIcon(QIcon(":/plomo/icon.png"));
    QGuiApplication::setApplicationDisplayName(QString());
    setWindowTitle("PLOMO");

    auto* panel = new PlomoBackdrop(ui->centralWidget, PlomoBackdrop::Mode::Home);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(40, 28, 40, 28);
    layout->setSpacing(0);

    // Top bar: emblem on the left, account and settings on the right.
    auto* top = new QHBoxLayout();
    top->setSpacing(10);
    auto* emblem = new QLabel(panel);
    emblem->setPixmap(QPixmap(":/plomo/icon.png").scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    top->addWidget(emblem);
    top->addStretch();
    m_plomoAccounts = new QPushButton(panel);
    m_plomoAccounts->setIconSize(QSize(20, 20));
    m_plomoAccounts->setCursor(Qt::PointingHandCursor);
    auto* settings = new QPushButton(tr("Configuración"), panel);
    settings->setCursor(Qt::PointingHandCursor);
    top->addWidget(m_plomoAccounts);
    top->addWidget(settings);
    layout->addLayout(top);

    auto* settingsMenu = new QMenu(settings);
    auto* languageAction = settingsMenu->addAction(tr("Idioma"));
    auto* memoryAction = settingsMenu->addAction(tr("Memoria"));
    m_plomoVerify = settingsMenu->addAction(tr("Verificar archivos"));
    m_plomoAlpha = settingsMenu->addAction(tr("Versiones alpha (pruebas)"));
    m_plomoAlpha->setCheckable(true);
    m_plomoAlpha->setChecked(m_plomoUpdater->alphaChannel());
    settingsMenu->addSeparator();
    settingsMenu->addAction(tr("Abrir carpeta del launcher"), this, &MainWindow::on_actionViewLauncherRootFolder_triggered);
    settings->setMenu(settingsMenu);

    layout->addStretch(1);

    // Left column: logo, status, progress and the main action.
    auto* column = new QWidget(panel);
    column->setFixedWidth(400);
    column->setAttribute(Qt::WA_TranslucentBackground);
    auto* col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(10);

    auto* logo = new QLabel(column);
    const qreal dpr = devicePixelRatioF();
    QPixmap logoPixmap = QPixmap(":/plomo/logo.png").scaledToWidth(qRound(380 * dpr), Qt::SmoothTransformation);
    logoPixmap.setDevicePixelRatio(dpr);
    logo->setPixmap(logoPixmap);
    col->addWidget(logo);
    col->addSpacing(18);

    m_plomoStatus = new QLabel(tr("Buscando actualizaciones..."), column);
    m_plomoStatus->setObjectName("plomoStatus");
    m_plomoStatus->setWordWrap(true);
    col->addWidget(m_plomoStatus);

    m_plomoProgress = new QProgressBar(column);
    m_plomoProgress->setRange(0, 100);
    m_plomoProgress->setValue(0);
    m_plomoProgress->hide();
    col->addWidget(m_plomoProgress);

    m_plomoAction = new QPushButton(tr("COMPROBANDO..."), column);
    m_plomoAction->setObjectName("plomoPlay");
    m_plomoAction->setCursor(Qt::PointingHandCursor);
    m_plomoAction->setEnabled(false);
    col->addWidget(m_plomoAction);

    auto* links = new QHBoxLayout();
    links->setSpacing(18);
    m_plomoCheck = new QPushButton(tr("Buscar actualizaciones"), column);
    m_plomoCheck->setObjectName("plomoLink");
    m_plomoCheck->setCursor(Qt::PointingHandCursor);
    m_plomoFolder = new QPushButton(tr("Abrir carpeta del juego"), column);
    m_plomoFolder->setObjectName("plomoLink");
    m_plomoFolder->setCursor(Qt::PointingHandCursor);
    m_plomoCancel = new QPushButton(tr("Cancelar"), column);
    m_plomoCancel->setObjectName("plomoLink");
    m_plomoCancel->setCursor(Qt::PointingHandCursor);
    m_plomoCancel->hide();
    links->addWidget(m_plomoCheck);
    links->addWidget(m_plomoFolder);
    links->addWidget(m_plomoCancel);
    links->addStretch();
    col->addLayout(links);

    m_plomoFooter = new QLabel(column);
    m_plomoFooter->setObjectName("plomoFooter");
    col->addSpacing(6);
    col->addWidget(m_plomoFooter);
    // Bottom row: the controls on the left, the "new launcher version" notice on the right.
    m_plomoUpdateNotice = new QPushButton(panel);
    m_plomoUpdateNotice->setObjectName("plomoNotice");
    m_plomoUpdateNotice->setCursor(Qt::PointingHandCursor);
    m_plomoUpdateNotice->hide();
    auto* bottom = new QHBoxLayout();
    bottom->addWidget(column, 0, Qt::AlignBottom);
    bottom->addStretch();
    bottom->addWidget(m_plomoUpdateNotice, 0, Qt::AlignBottom | Qt::AlignRight);
    layout->addLayout(bottom);
    connect(m_plomoUpdateNotice, &QPushButton::clicked, this, [this] {
        if (m_plomoSelfUpdateReady)
            startLauncherSelfUpdate();
        else
            DesktopServices::openUrl(QUrl(m_plomoDownloadUrl));
    });

    connect(m_plomoAction, &QPushButton::clicked, this, [this] {
        using State = PlomoPackUpdater::State;
        const auto state = m_plomoUpdater->state();
        if (state == State::Ready || (state == State::Failed && m_plomoUpdater->canPlayOffline())) {
            setSelectedInstanceById("PLOMO");
            if (m_selectedInstance && !m_selectedInstance->isRunning() && !m_plomoLaunching)
                APPLICATION->launch(m_selectedInstance);
            else if (!m_selectedInstance)
                m_plomoStatus->setText(tr("No se encontró la instancia PLOMO. Prueba a buscar actualizaciones."));
        } else if (state == State::Failed) {
            m_plomoUpdater->check();
        } else {
            m_plomoUpdater->install();
        }
    });
    connect(m_plomoCheck, &QPushButton::clicked, m_plomoUpdater, &PlomoPackUpdater::check);
    connect(m_plomoCancel, &QPushButton::clicked, this, [this] {
        if (m_plomoPreparing) {
            if (m_plomoPrepCurrent && m_plomoPrepCurrent->canAbort())
                m_plomoPrepCurrent->abort();
            return;
        }
        if (m_plomoLaunchTask && m_plomoLaunchTask->canAbort())
            m_plomoLaunchTask->abort();
    });
    connect(APPLICATION, &Application::launchActiveChanged, this, [this](bool active) {
        m_plomoLaunching = active;
        if (!active)
            m_plomoLaunchTask = nullptr;
        updatePlomoUi();
    });
    connect(APPLICATION, &Application::launchProgressRequested, this, &MainWindow::showLaunchProgress);
    connect(APPLICATION, &Application::gameLaunching, this, &QWidget::showMinimized);
    connect(m_plomoFolder, &QPushButton::clicked, this, [this] { DesktopServices::openPath(QFileInfo(m_plomoRoot)); });

    ui->horizontalLayout->setContentsMargins(0, 0, 0, 0);
    // Home plus PLOMO's own screens; the stock settings dialog is never shown.
    auto* stack = new QStackedWidget(ui->centralWidget);
    stack->setStyleSheet(plomoStyleSheet());
    stack->addWidget(panel);
    m_plomoStack = stack;
    m_plomoHome = panel;
    m_plomoUpdateScreen = new PlomoUpdateScreen(stack);
    stack->addWidget(m_plomoUpdateScreen);
    m_plomoSelfUpdater = new PlomoSelfUpdater(APPLICATION->network(), this);
    connect(m_plomoSelfUpdater, &PlomoSelfUpdater::progressChanged, this, [this](int percent) {
        m_plomoUpdateScreen->setProgress(percent);
        m_plomoUpdateScreen->setStatus(percent >= 0 ? tr("Descargando la actualización... %1%").arg(percent)
                                                    : tr("Descargando la actualización..."));
    });
    connect(m_plomoSelfUpdater, &PlomoSelfUpdater::failed, this, [this](const QString& message) {
        m_plomoStack->setCurrentWidget(m_plomoHome);
        QMessageBox::warning(this, tr("Actualizar el launcher"), message);
    });
    connect(m_plomoSelfUpdater, &PlomoSelfUpdater::launched, this, [this] {
        m_plomoUpdateScreen->setProgress(100);
        m_plomoUpdateScreen->setStatus(tr("Instalando... el launcher se reiniciará."));
        QTimer::singleShot(400, qApp, &QCoreApplication::quit);  // the installer needs the files unlocked
    });
    auto* accountsPage = new PlomoAccountsPage(stack);
    auto* languagePage = new PlomoLanguagePage(stack);
    auto* memoryPage = new PlomoMemoryPage(stack);
    for (PlomoPage* page : {static_cast<PlomoPage*>(accountsPage), static_cast<PlomoPage*>(languagePage),
                            static_cast<PlomoPage*>(memoryPage)}) {
        stack->addWidget(page);
        connect(page, &PlomoPage::backRequested, stack, [stack, panel] { stack->setCurrentWidget(panel); });
    }
    auto showPage = [stack](PlomoPage* page) {
        page->refresh();
        stack->setCurrentWidget(page);
    };
    connect(m_plomoAccounts, &QPushButton::clicked, this, [=] { showPage(accountsPage); });
    connect(languageAction, &QAction::triggered, this, [=] { showPage(languagePage); });
    connect(memoryAction, &QAction::triggered, this, [=] { showPage(memoryPage); });
    ui->horizontalLayout->addWidget(stack, 1);
    view->hide();
    ui->mainToolBar->hide();
    ui->instanceToolBar->hide();
    ui->newsToolBar->hide();
    menuBar()->hide();
    statusBar()->hide();
    setMinimumSize(820, 500);
    resize(960, 560);

    connect(m_plomoUpdater, &PlomoPackUpdater::stateChanged, this, &MainWindow::updatePlomoUi);
    connect(m_plomoVerify, &QAction::triggered, m_plomoUpdater, &PlomoPackUpdater::verify);
    connect(m_plomoAlpha, &QAction::triggered, this, [this](bool alpha) {
        if (alpha && QMessageBox::warning(this, tr("Versiones alpha"),
                                          tr("Las versiones alpha son de pruebas y pueden fallar o tener partidas incompletas. "
                                             "Actívalas solo si vas a probar el juego con otros jugadores. ¿Activarlas?"),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            m_plomoAlpha->setChecked(false);
            return;
        }
        APPLICATION->settings()->set("PlomoAlphaChannel", alpha);
        m_plomoUpdater->setAlphaChannel(alpha);
        m_plomoUpdater->check();  // the other channel may carry another version: the button turns into UPDATE
    });
    connect(m_plomoUpdater, &PlomoPackUpdater::verifyProgressChanged, this, [this](int percent) {
        m_plomoProgress->setValue(percent);
        m_plomoStatus->setText(tr("Verificando archivos... %1%").arg(percent));
    });
    connect(m_plomoUpdater, &PlomoPackUpdater::verified, this, [this](int mismatches) {
        m_plomoStatus->setText(mismatches == 0   ? tr("Todos los archivos están correctos.")
                               : mismatches == 1 ? tr("Se encontró 1 archivo dañado, modificado o ausente.")
                                                 : tr("Se encontraron %1 archivos dañados, modificados o ausentes.").arg(mismatches));
    });
    connect(m_plomoUpdater, &PlomoPackUpdater::progressChanged, this, [this](qint64 done, qint64 total) {
        m_plomoProgress->setValue(total > 0 ? qRound(100.0 * done / total) : 100);
        constexpr double MiB = 1024.0 * 1024.0;
        m_plomoStatus->setText(tr("Descargando y verificando archivos... %1 / %2 MB")
                                   .arg(done / MiB, 0, 'f', 0)
                                   .arg(total / MiB, 0, 'f', 0));
    });
    connect(m_plomoUpdater, &PlomoPackUpdater::errorOccurred, this, [this](const QString& error) {
        m_plomoStatus->setToolTip(error);
        // With a previous install and no server, the status already explains that the game can still be played.
        if (!m_plomoUpdater->canPlayOffline())
            m_plomoStatus->setText(error);
    });
    connect(m_plomoUpdater, &PlomoPackUpdater::installed, this, [this] {
        refreshInstances();
        setSelectedInstanceById("PLOMO");
        startMinecraftPrepare();
    });

    auto* accounts = APPLICATION->accounts();
    connect(accounts, &AccountList::defaultAccountChanged, this, &MainWindow::updatePlomoAccount);
    connect(accounts, &AccountList::listChanged, this, &MainWindow::updatePlomoAccount);
    connect(accounts, &AccountList::listActivityChanged, this, &MainWindow::updatePlomoAccount);
    updatePlomoAccount();
    updatePlomoUi();
    QTimer::singleShot(0, m_plomoUpdater, &PlomoPackUpdater::check);
    QTimer::singleShot(0, this, &MainWindow::checkLauncherUpdate);
}

void MainWindow::updatePlomoAccount()
{
    const auto account = APPLICATION->accounts()->defaultAccount();
    if (account && !account->profileName().isEmpty()) {
        m_plomoAccounts->setText(account->profileName());
        const auto face = account->getFace();
        m_plomoAccounts->setIcon(face.isNull() ? QIcon() : QIcon(face));
    } else {
        m_plomoAccounts->setText(tr("Iniciar sesión"));
        m_plomoAccounts->setIcon(QIcon());
    }
}

// Tells the player when a newer launcher exists. Nothing is downloaded or installed here: the notice only opens the
// download page. launcher.json looks like {"version": "1.0.1", "download_url": "https://playplomo.com/..."}.
void MainWindow::checkLauncherUpdate()
{
    // A staging address (hidden setting) lets the self-update be tested without touching the real launcher.json.
    const QString infoOverride = APPLICATION->settings()->get("PlomoLauncherInfoUrl").toString();
    const bool staging = !infoOverride.isEmpty();
    QNetworkRequest request{QUrl(staging ? infoOverride : QString::fromLatin1(kLauncherInfoUrl))};
    request.setTransferTimeout(8000);
    auto* reply = APPLICATION->network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, staging] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // no connection or no launcher.json yet: stay silent
        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const auto remote = QVersionNumber::fromString(object.value("version").toString());
        const QVersionNumber current(BuildConfig.VERSION_MAJOR, BuildConfig.VERSION_MINOR, BuildConfig.VERSION_PATCH);
        if (remote.isNull() || remote <= current)
            return;
        // Only web links are followed; anything else falls back to the project site.
        QUrl url(object.value("download_url").toString());
        if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http"))
            url = QUrl(QString::fromLatin1(kDownloadPageFallback));
        m_plomoDownloadUrl = url.toString();
        // Self-update needs the installer address and its SHA-256 in launcher.json, and an installed (not portable) launcher.
        const QUrl installerUrl(object.value("installer_url").toString());
        const QString sha256 = object.value("sha256").toString().toLower();
        m_plomoSelfUpdateReady = PlomoSelfUpdater::canSelfUpdate() && PlomoSelfUpdater::isTrustedUrl(installerUrl, staging) &&
                                 PlomoSelfUpdater::isValidSha256(sha256);
        m_plomoInstallerUrl = installerUrl.toString();
        m_plomoInstallerSha = sha256;
        m_plomoRemoteVersion = remote.toString();
        m_plomoUpdateNotice->setText(m_plomoSelfUpdateReady ? tr("Actualizar el launcher a v%1").arg(remote.toString())
                                                            : tr("Nueva versión del launcher disponible: v%1").arg(remote.toString()));
        m_plomoUpdateNotice->show();
    });
}

void MainWindow::startLauncherSelfUpdate()
{
    using State = PlomoPackUpdater::State;
    const auto state = m_plomoUpdater->state();
    const bool busy = m_plomoPreparing || m_plomoLaunching || (m_selectedInstance && m_selectedInstance->isRunning()) ||
                      state == State::Installing || state == State::Verifying || state == State::Checking;
    if (busy) {
        QMessageBox::information(this, tr("Actualizar el launcher"),
                                 tr("Espera a que termine lo que está en curso (descarga, verificación o partida) y vuelve a pulsar el aviso de actualización."));
        return;
    }
    m_plomoUpdateScreen->setStatus(tr("Descargando la actualización..."));
    m_plomoUpdateScreen->setProgress(0);
    m_plomoStack->setCurrentWidget(m_plomoUpdateScreen);
    m_plomoSelfUpdater->start(QUrl(m_plomoInstallerUrl), m_plomoInstallerSha, m_plomoRemoteVersion);
}

void MainWindow::startMinecraftPrepare()
{
    auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById("PLOMO"));
    if (!instance || m_plomoPreparing)
        return;
    // Same order as a real launch: metadata, Java, then libraries and assets.
    m_plomoPrepLaunch = std::shared_ptr<LaunchTask>(LaunchTask::create(instance));
    m_plomoPrepSteps.clear();
    m_plomoPrepSteps.push_back(makeShared<MinecraftLoadAndCheck>(instance, Net::Mode::Online));
    // The Java step reports progress through its LaunchTask, which indexes its step list: it must contain the step.
    auto javaStep = makeShared<AutoInstallJava>(m_plomoPrepLaunch.get());
    m_plomoPrepLaunch->appendStep(javaStep);
    m_plomoPrepSteps.push_back(javaStep);
    for (const auto& step : instance->createUpdateTask())
        m_plomoPrepSteps.push_back(step);
    m_plomoPrepIndex = 0;
    m_plomoPreparing = true;
    m_plomoProgress->setValue(0);
    m_plomoStatus->setText(tr("Preparando Minecraft..."));
    updatePlomoUi();
    runNextPrepStep();
}

void MainWindow::runNextPrepStep()
{
    if (m_plomoPrepIndex >= m_plomoPrepSteps.size()) {
        finishPrepare({});
        return;
    }
    const auto task = m_plomoPrepSteps.at(m_plomoPrepIndex);
    const int index = m_plomoPrepIndex;
    const int count = m_plomoPrepSteps.size();
    m_plomoPrepCurrent = task.get();
    auto refresh = [this, task, index, count] {
        if (m_plomoPrepCurrent != task.get())
            return;
        const auto total = task->getTotalProgress();
        const double part = total > 0 ? double(task->getProgress()) / double(total) : 0.0;
        m_plomoProgress->setValue(qRound(100.0 * (index + part) / count));
        const auto status = task->getStatus();
        m_plomoStatus->setText(status.isEmpty() ? tr("Preparando Minecraft...") : status);
        m_plomoCancel->setEnabled(task->canAbort());
    };
    connect(task.get(), &Task::status, this, refresh);
    connect(task.get(), &Task::progress, this, refresh);
    connect(task.get(), &Task::abortStatusChanged, this, refresh);
    const QString name = QString::fromLatin1(task->metaObject()->className());
    qInfo() << "PLOMO prepare step" << index << name << "start";
    // A step can report completion more than once (AutoInstallJava does), and a step that is not the current one
    // must never advance the chain: each handler only counts while its own step is the active one.
    connect(task.get(), &Task::succeeded, this, [this, index, name] {
        if (!m_plomoPreparing || m_plomoPrepIndex != index)
            return;
        qInfo() << "PLOMO prepare step" << index << name << "succeeded";
        ++m_plomoPrepIndex;
        runNextPrepStep();
    });
    connect(task.get(), &Task::failed, this, [this, index, name](const QString& reason) {
        if (!m_plomoPreparing || m_plomoPrepIndex != index)
            return;
        qInfo() << "PLOMO prepare step" << index << name << "failed" << reason;
        finishPrepare(reason);
    });
    connect(task.get(), &Task::aborted, this, [this, index, name] {
        if (!m_plomoPreparing || m_plomoPrepIndex != index)
            return;
        qInfo() << "PLOMO prepare step" << index << name << "aborted";
        finishPrepare(tr("Cancelado"));
    });
    task->start();
    refresh();
}

void MainWindow::finishPrepare(const QString& error)
{
    m_plomoPreparing = false;
    m_plomoPrepCurrent = nullptr;
    // The finished task is still emitting this signal; release it on the next turn of the event loop.
    QTimer::singleShot(0, this, [this] {
        m_plomoPrepSteps.clear();
        m_plomoPrepLaunch.reset();
    });
    updatePlomoUi();
    if (!error.isEmpty()) {
        qWarning() << "PLOMO prepare failed:" << error;
        m_plomoStatus->setText(tr("PLOMO está instalado, pero Minecraft no terminó de prepararse (%1). Se reintentará al pulsar Jugar.").arg(error));
    }
}

void MainWindow::showLaunchProgress(Task* task)
{
    m_plomoLaunchTask = task;
    auto refresh = [this, task] {
        if (m_plomoLaunchTask != task)
            return;
        const auto total = task->getTotalProgress();
        m_plomoProgress->setValue(total > 0 ? qRound(100.0 * task->getProgress() / total) : 0);
        const auto status = task->getStatus();
        if (!status.isEmpty())
            m_plomoStatus->setText(status);
        m_plomoCancel->setEnabled(task->canAbort());
    };
    connect(task, &Task::status, this, refresh);
    connect(task, &Task::progress, this, refresh);
    connect(task, &Task::abortStatusChanged, this, refresh);
    connect(task, &Task::finished, this, [this, task] {
        if (m_plomoLaunchTask == task)
            m_plomoLaunchTask = nullptr;
    });
    m_plomoProgress->setValue(0);
    updatePlomoUi();
    refresh();
}

void MainWindow::updatePlomoUi()
{
    using State = PlomoPackUpdater::State;
    const auto state = m_plomoUpdater->state();
    const bool running = m_selectedInstance && m_selectedInstance->isRunning();
    const bool offlinePlay = state == State::Failed && m_plomoUpdater->canPlayOffline();
    // The footer shows what is installed, not what the server offers (the update button already says that).
    const auto version = m_plomoUpdater->isInstalled() ? m_plomoUpdater->installedVersion() : QString();

    m_plomoProgress->setVisible(state == State::Installing || state == State::Verifying);
    m_plomoVerify->setEnabled(m_plomoUpdater->isInstalled() && !m_plomoPreparing && !m_plomoLaunching &&
                              (state == State::Ready || state == State::Update || state == State::Failed));
    m_plomoCheck->setEnabled(state == State::Ready || state == State::Failed);
    m_plomoFolder->setEnabled(m_plomoUpdater->isInstalled());
    const bool alphaInstalled = m_plomoUpdater->isInstalled() && m_plomoUpdater->installedChannel() == "alpha";
    m_plomoFooter->setText(version.isEmpty() ? QString()
                           : alphaInstalled  ? tr("PLOMO v%1 (alpha)").arg(version)
                                             : tr("PLOMO v%1").arg(version));
    // Switching channel while something is running would change the manifest under it.
    m_plomoAlpha->setEnabled(!m_plomoPreparing && !m_plomoLaunching && !running &&
                             (state == State::Ready || state == State::Update || state == State::Download || state == State::Failed));

    // While the game is being prepared everything stays in this window; the stock progress dialog is not used.
    // Prism flags the instance as running as soon as it starts preparing it, so "starting" is tracked by the launch signals instead.
    if (m_plomoPreparing) {
        m_plomoCancel->setVisible(true);
        m_plomoProgress->setVisible(true);
        m_plomoAction->setText(tr("INSTALANDO..."));
        m_plomoAction->setEnabled(false);
        m_plomoCheck->setEnabled(false);
        return;
    }
    const bool launching = m_plomoLaunching;
    m_plomoCancel->setVisible(launching);
    if (launching) {
        m_plomoProgress->setVisible(true);
        m_plomoAction->setText(tr("INICIANDO..."));
        m_plomoAction->setEnabled(false);
        m_plomoCheck->setEnabled(false);
        if (!m_plomoLaunchTask)
            m_plomoStatus->setText(tr("Preparando el juego..."));
        return;
    }

    if (state != State::Failed)
        m_plomoStatus->setToolTip({});
    m_plomoAction->setEnabled(state != State::Checking && state != State::Installing && state != State::Verifying);

    switch (state) {
        case State::Checking:
            m_plomoAction->setText(tr("COMPROBANDO..."));
            m_plomoStatus->setText(tr("Buscando actualizaciones..."));
            break;
        case State::Download:
            m_plomoAction->setText(tr("DESCARGAR"));
            m_plomoStatus->setText(tr("PLOMO aún no está instalado."));
            break;
        case State::Update:
            m_plomoAction->setText(tr("ACTUALIZAR"));
            m_plomoStatus->setText(tr("Hay una actualización disponible."));
            break;
        case State::Ready:
            m_plomoAction->setText(running ? tr("EN JUEGO") : tr("JUGAR"));
            m_plomoStatus->setText(tr("PLOMO está actualizado."));
            break;
        case State::Installing:
            m_plomoAction->setText(tr("INSTALANDO..."));
            m_plomoStatus->setText(tr("Descargando y verificando archivos..."));
            break;
        case State::Verifying:
            m_plomoAction->setText(tr("VERIFICANDO..."));
            m_plomoStatus->setText(tr("Verificando archivos... %1%").arg(m_plomoProgress->value()));
            break;
        case State::Failed:
            m_plomoAction->setText(offlinePlay ? (running ? tr("EN JUEGO") : tr("JUGAR")) : tr("REINTENTAR"));
            if (offlinePlay)
                m_plomoStatus->setText(tr("Sin conexión con el servidor de PLOMO. Se jugará la versión instalada."));
            break;
    }
    if ((state == State::Ready || offlinePlay) && running)
        m_plomoAction->setEnabled(false);
}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());

    filteredMenu->addAction(ui->actionToggleStatusBar);
    filteredMenu->addAction(ui->actionLockToolbars);

    return filteredMenu;
}
void MainWindow::setStatusBarVisibility(bool state)
{
    statusBar()->setVisible(state);
    APPLICATION->settings()->set("StatusBarVisible", state);
}
void MainWindow::lockToolbars(bool state)
{
    ui->mainToolBar->setMovable(!state);
    ui->instanceToolBar->setMovable(!state);
    ui->newsToolBar->setMovable(!state);
    APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::konamiTriggered()
{
    QString gradient =
        " stop:0 rgba(125, 0, 0, 255), stop:0.166 rgba(125, 125, 0, 255), stop:0.333 rgba(0, 125, 0, 255), stop:0.5 rgba(0, 125, 125, "
        "255), stop:0.666 rgba(0, 0, 125, 255), stop:0.833 rgba(125, 0, 125, 255), stop:1 rgba(125, 0, 0, 255));";
    QString stylesheet = "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0," + gradient;
    if (ui->mainToolBar->styleSheet() == stylesheet) {
        ui->mainToolBar->setStyleSheet("");
        ui->instanceToolBar->setStyleSheet("");
        ui->centralWidget->setStyleSheet("");
        ui->newsToolBar->setStyleSheet("");
        ui->statusBar->setStyleSheet("");
        qDebug() << "Super Secret Mode DEACTIVATED!";
    } else {
        ui->mainToolBar->setStyleSheet(stylesheet);
        ui->instanceToolBar->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1," + gradient);
        ui->centralWidget->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1," + gradient);
        ui->newsToolBar->setStyleSheet(stylesheet);
        ui->statusBar->setStyleSheet(stylesheet);
        qDebug() << "Super Secret Mode ACTIVATED!";
    }
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        actions.prepend(ui->actionChangeInstIcon);
        actions.prepend(ui->actionRenameInstance);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    ui->menuBar->setVisible(APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
    ui->mainToolBar->setVisible(ui->menuBar->isNativeMenuBar() || !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
    ui->actionCAT->setVisible(APPLICATION->settings()->get("CatButtonVisible").toBool());
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    for (auto* theme : themes) {
        QAction* themeAction = themeMenu->addAction(theme->name());

        themeAction->setCheckable(true);
        if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
            themeAction->setChecked(true);
        }
        themeAction->setActionGroup(themesGroup);

        connect(themeAction, &QAction::triggered, [theme]() {
            APPLICATION->themeManager()->setApplicationTheme(theme->id());
            APPLICATION->settings()->set("ApplicationTheme", theme->id());
        });
    }

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();
    
    bool canChangeSkin = defaultAccount && (defaultAccount->accountType() == AccountType::MSA) && !defaultAccount->isActive();
    ui->actionManageSkins->setEnabled(canChangeSkin);

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(QIcon::fromTheme("noaccount"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    ui->accountsMenu->addAction(ui->actionManageSkins);
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
    ui->actionAccountsButton->setText(tr("Accounts"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            secretEventFilter->input(ev);
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    if (m_newsChecker->isLoadingNews()) {
        newsLabel->setText(tr("Loading news..."));
        newsLabel->setEnabled(false);
        ui->actionMoreNews->setVisible(false);
    } else {
        QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
        if (entries.length() > 0) {
            newsLabel->setText(entries[0]->title);
            newsLabel->setEnabled(true);
            ui->actionMoreNews->setVisible(true);
        } else {
            newsLabel->setText(tr("No news available."));
            newsLabel->setEnabled(false);
            ui->actionMoreNews->setVisible(false);
        }
    }
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::onCatToggled(bool state)
{
    setCatBackground(state);
    APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
    view->setPaintCat(enabled);
    view->viewport()->repaint();
}

void MainWindow::runModalTask(Task* task)
{
    connect(task, &Task::failed,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task, &Task::succeeded, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    connect(task, &Task::aborted, [this] {
        CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)
            ->show();
    });
    ProgressDialog loadDialog(this);
    loadDialog.setSkipButton(true, tr("Abort"));
    loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    runModalTask(task.get());
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(copyTask));
    runModalTask(task.get());
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        if (url.isEmpty() || url.toString().trimmed().isEmpty())
            continue;

        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify

            const bool isExternalURLImport = (url.host().toLower() == "import") || (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "prismlauncher" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) && isExternalURLImport) {
                // PrismLauncher URL protocol modpack import
                // works for any prism fork
                // preferred import format: prismlauncher://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: prismlauncher://import/ENCODED
                if (encodedTarget.isEmpty()) {
                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: missing 'url' parameter."),
                                                 QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: URL must be http(s)."), QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this, tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2").arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiDownload::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.setSkipButton(true, tr("Abort"));
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        if (localFileName.isEmpty() || !localFileInfo.exists()) {
            qDebug() << "Ignoring invalid path" << localFileName;
            continue;
        }

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = dynamic_cast<MinecraftInstance*>(inst);

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResource(localFileName);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResource(localFileName);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResource(localFileName);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResource(localFileName);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        changeIconButton->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        auto new_icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
        ui->actionChangeInstIcon->setIcon(new_icon);
        changeIconButton->setIcon(new_icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Group name"), tr("Enter a new group name."), groups, index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        APPLICATION->instances()->setInstanceGroup(instId, dst);
    }
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    const int reply = QMessageBox::question(this, tr("Delete group"), tr("Are you sure you want to delete the group '%1'?").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename group"), tr("Enter a new group name."), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = APPLICATION->instances()->getGroups().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename group"), empty ? tr("Cannot set empty name.") : tr("Group already exists. :/"));
        return;
    }

    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
    // FIXME: quick HACK to make this work. improve, optimize.
    APPLICATION->instances()->loadList();
    proxymodel->invalidate();
    proxymodel->sort(0);
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but Prism Launcher exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    if (m_plomoUpdater) {
        ui->mainToolBar->hide();
        ui->instanceToolBar->hide();
        ui->newsToolBar->hide();
        menuBar()->hide();
        statusBar()->hide();
    }
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageSkins_triggered()
{
    auto account = APPLICATION->accounts()->defaultAccount();

    if (account && (account->accountType() == AccountType::MSA) && !account->isActive()) {
        SkinManageDialog dialog(this, account);
        dialog.exec();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.exec();
}

void MainWindow::newsButtonClicked()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.toggleArticleList();
    news_dialog.exec();
}

void MainWindow::onCatChanged(int)
{
    setCatBackground(APPLICATION->settings()->get("TheCat").toBool());
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    if (m_selectedInstance) {
        view->edit(view->currentIndex());
    }
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    instanceToolbarSetting->set(QString::fromUtf8(ui->instanceToolBar->getVisibilityState().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    BaseInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    activateInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        APPLICATION->launch(m_selectedInstance);
    }
}

void MainWindow::activateInstance(BaseInstance* instance)
{
    APPLICATION->launch(instance);
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        ui->instanceToolBar->setEnabled(true);
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        renameButton->setText(m_selectedInstance->name());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    ui->instanceToolBar->setEnabled(false);
    setInstanceActionsEnabled(false);
    updateLaunchButton();
    renameButton->setText(tr("Rename Instance"));
    updateInstanceToolIcon("grass");

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());

    int timePlayed = APPLICATION->instances()->getTotalPlayTime();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
    if (m_plomoUpdater)
        updatePlomoUi();
}
