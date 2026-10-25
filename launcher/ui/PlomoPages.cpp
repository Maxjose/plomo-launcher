// SPDX-License-Identifier: GPL-3.0-only
#include "PlomoPages.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QPainter>
#include <QProcess>
#include <QTranslator>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "HardwareInfo.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "translations/TranslationsModel.h"
#include "ui/dialogs/ChooseOfflineNameDialog.h"
#include "ui/dialogs/MSALoginDialog.h"

namespace {
class PlomoEnglish : public QTranslator {
   public:
    QString translate(const char*, const char* sourceText, const char*, int) const override
    {
        static const QHash<QString, QString> table = {
        {QStringLiteral("ACTUALIZAR"), QStringLiteral("UPDATE")},
        {QStringLiteral("Abrir carpeta del juego"), QStringLiteral("Open game folder")},
        {QStringLiteral("Abrir carpeta del launcher"), QStringLiteral("Open launcher folder")},
        {QStringLiteral("Buscando actualizaciones..."), QStringLiteral("Checking for updates...")},
        {QStringLiteral("Buscar actualizaciones"), QStringLiteral("Check for updates")},
        {QStringLiteral("COMPROBANDO..."), QStringLiteral("CHECKING...")},
        {QStringLiteral("Configuración"), QStringLiteral("Settings")},
        {QStringLiteral("DESCARGAR"), QStringLiteral("DOWNLOAD")},
        {QStringLiteral("Descargando y verificando archivos... %1 / %2 MB"), QStringLiteral("Downloading and verifying files... %1 / %2 MB")},
        {QStringLiteral("Descargando y verificando archivos..."), QStringLiteral("Downloading and verifying files...")},
        {QStringLiteral("EN JUEGO"), QStringLiteral("IN GAME")},
        {QStringLiteral("Hay una actualización disponible."), QStringLiteral("An update is available.")},
        {QStringLiteral("INSTALANDO..."), QStringLiteral("INSTALLING...")},
        {QStringLiteral("Idioma"), QStringLiteral("Language")},
        {QStringLiteral("Iniciar sesión"), QStringLiteral("Sign in")},
        {QStringLiteral("JUGAR"), QStringLiteral("PLAY")},
        {QStringLiteral("Memoria"), QStringLiteral("Memory")},
        {QStringLiteral("No se encontró la instancia PLOMO. Prueba a buscar actualizaciones."), QStringLiteral("PLOMO instance not found. Try checking for updates.")},
        {QStringLiteral("PLOMO aún no está instalado."), QStringLiteral("PLOMO is not installed yet.")},
        {QStringLiteral("PLOMO está actualizado."), QStringLiteral("PLOMO is up to date.")},
        {QStringLiteral("PLOMO v%1  ·  "), QStringLiteral("PLOMO v%1  ·  ")},
        {QStringLiteral("REINTENTAR"), QStringLiteral("RETRY")},
        {QStringLiteral("Sin conexión con el servidor de PLOMO. Se jugará la versión instalada."), QStringLiteral("Can't reach the PLOMO server. The installed version will be played.")},
        {QStringLiteral("El servidor de PLOMO devolvió datos no válidos"), QStringLiteral("The PLOMO server returned invalid data")},
        {QStringLiteral("El servidor de PLOMO publicó un archivo con datos no válidos"), QStringLiteral("The PLOMO server published a file with invalid data")},
        {QStringLiteral("Falló la descarga de %1: %2"), QStringLiteral("Download of %1 failed: %2")},
        {QStringLiteral("La dirección de descarga de PLOMO no es válida"), QStringLiteral("The PLOMO download address is not valid")},
        {QStringLiteral("La verificación de %1 falló; vuelve a intentarlo"), QStringLiteral("Verification of %1 failed; try again")},
        {QStringLiteral("La versión publicada de PLOMO no es compatible con este launcher"), QStringLiteral("The published PLOMO version is not compatible with this launcher")},
        {QStringLiteral("No se pudieron crear los metadatos de la instancia PLOMO"), QStringLiteral("Could not create the PLOMO instance metadata")},
        {QStringLiteral("No se pudo conectar con el servidor de PLOMO: %1"), QStringLiteral("Could not connect to the PLOMO server: %1")},
        {QStringLiteral("No se pudo crear la carpeta para %1"), QStringLiteral("Could not create the folder for %1")},
        {QStringLiteral("No se pudo crear la configuración de la instancia PLOMO"), QStringLiteral("Could not create the PLOMO instance settings")},
        {QStringLiteral("No se pudo escribir %1"), QStringLiteral("Could not write %1")},
        {QStringLiteral("No se pudo guardar la versión instalada de PLOMO"), QStringLiteral("Could not save the installed PLOMO version")},
        {QStringLiteral("No se pudo instalar %1"), QStringLiteral("Could not install %1")},
        {QStringLiteral("%1   ★ En uso"), QStringLiteral("%1   ★ In use")},
        {QStringLiteral("%1 GB"), QStringLiteral("%1 GB")},
        {QStringLiteral("Actualizar"), QStringLiteral("Refresh")},
        {QStringLiteral("Añadir cuenta Microsoft"), QStringLiteral("Add Microsoft account")},
        {QStringLiteral("Añadir cuenta offline"), QStringLiteral("Add offline account")},
        {QStringLiteral("Comprobando..."), QStringLiteral("Checking...")},
        {QStringLiteral("Conectada"), QStringLiteral("Connected")},
        {QStringLiteral("Cuentas"), QStringLiteral("Accounts")},
        {QStringLiteral("Guardado. Se aplicará la próxima vez que inicies el juego."), QStringLiteral("Saved. It will apply the next time you start the game.")},
        {QStringLiteral("Guardar"), QStringLiteral("Save")},
        {QStringLiteral("Idioma del launcher."), QStringLiteral("Launcher language.")},
        {QStringLiteral("Inicia sesión con la cuenta de Minecraft con la que vas a jugar."), QStringLiteral("Sign in with the Minecraft account you will play with.")},
        {QStringLiteral("Memoria total del equipo: %1 GB"), QStringLiteral("Total system memory: %1 GB")},
        {QStringLiteral("Quitar cuenta"), QStringLiteral("Remove account")},
        {QStringLiteral("Quitar"), QStringLiteral("Remove")},
        {QStringLiteral("RAM que el juego puede usar. Más memoria no siempre es mejor: deja libre la que necesite Windows."), QStringLiteral("RAM the game can use. More is not always better: leave enough free for Windows.")},
        {QStringLiteral("Requiere iniciar sesión de nuevo"), QStringLiteral("Needs to sign in again")},
        {QStringLiteral("Sin comprobar"), QStringLiteral("Not checked")},
        {QStringLiteral("Sin conexión"), QStringLiteral("Offline")},
        {QStringLiteral("Todavía no hay ninguna cuenta. Pulsa «Añadir cuenta Microsoft»."), QStringLiteral("No accounts yet. Click \"Add Microsoft account\".")},
        {QStringLiteral("Usar esta cuenta"), QStringLiteral("Use this account")},
        {QStringLiteral("¿Seguro que quieres quitar esta cuenta del launcher?"), QStringLiteral("Are you sure you want to remove this account from the launcher?")},
        {QStringLiteral("←  Volver"), QStringLiteral("←  Back")},
        {QStringLiteral("Aplicar"), QStringLiteral("Apply")},
        {QStringLiteral("El launcher se reiniciará para aplicar el idioma."), QStringLiteral("The launcher will restart to apply the language.")},
        {QStringLiteral("Debes añadir primero una cuenta Microsoft que posea Minecraft antes de poder añadir una cuenta offline."), QStringLiteral("You must add a Microsoft account that owns Minecraft before adding an offline account.")},
        {QStringLiteral("Escribe el nombre de usuario de tu cuenta offline."), QStringLiteral("Enter the username for your offline account.")},
        {QStringLiteral("Error"), QStringLiteral("Error")},
        {QStringLiteral("Cancelar"), QStringLiteral("Cancel")},
        {QStringLiteral("Nueva versión del launcher disponible: v%1"), QStringLiteral("New launcher version available: v%1")},
        {QStringLiteral("Verificar archivos"), QStringLiteral("Verify files")},
        {QStringLiteral("Versiones alpha (pruebas)"), QStringLiteral("Alpha versions (testing)")},
        {QStringLiteral("Versiones alpha"), QStringLiteral("Alpha versions")},
        {QStringLiteral("Las versiones alpha son de pruebas y pueden fallar o tener partidas incompletas. Actívalas solo si vas a probar el juego con otros jugadores. ¿Activarlas?"), QStringLiteral("Alpha versions are for testing and may break or have unfinished game modes. Turn them on only if you are going to test the game with other players. Turn them on?")},
        {QStringLiteral("PLOMO v%1 (alpha)"), QStringLiteral("PLOMO v%1 (alpha)")},
        {QStringLiteral("VERIFICANDO..."), QStringLiteral("VERIFYING...")},
        {QStringLiteral("Verificando archivos... %1%"), QStringLiteral("Verifying files... %1%")},
        {QStringLiteral("Todos los archivos están correctos."), QStringLiteral("All files are correct.")},
        {QStringLiteral("Se encontró 1 archivo dañado, modificado o ausente."), QStringLiteral("Found 1 damaged, modified or missing file.")},
        {QStringLiteral("Se encontraron %1 archivos dañados, modificados o ausentes."), QStringLiteral("Found %1 damaged, modified or missing files.")},
        {QStringLiteral("Preparando Minecraft..."), QStringLiteral("Preparing Minecraft...")},
        {QStringLiteral("PLOMO está instalado, pero Minecraft no terminó de prepararse (%1). Se reintentará al pulsar Jugar."), QStringLiteral("PLOMO is installed, but Minecraft did not finish preparing (%1). It will retry when you press Play.")},
        {QStringLiteral("INICIANDO..."), QStringLiteral("STARTING...")},
        {QStringLiteral("Preparando el juego..."), QStringLiteral("Preparing the game...")},
        };
        return table.value(QString::fromUtf8(sourceText));
    }
    bool isEmpty() const override { return false; }
};
PlomoEnglish* g_english = nullptr;
}  // namespace

void plomoInstallLanguage(const QString& languageKey)
{
    if (!g_english)
        g_english = new PlomoEnglish();
    QCoreApplication::removeTranslator(g_english);
    if (languageKey.startsWith("en"))
        QCoreApplication::installTranslator(g_english);
}

QString plomoStyleSheet()
{
    return R"(
QLabel#plomoStatus { color: #eceff2; font-size: 14px; }
QLabel#plomoFooter { color: rgba(255,255,255,140); font-size: 11px; }
QLabel#plomoTitle { color: #ffffff; font-size: 26px; font-weight: 800; }
QLabel#plomoHint { color: rgba(255,255,255,150); font-size: 12px; }
QLabel#plomoValue { color: #ffffff; font-size: 22px; font-weight: 700; }
QFrame#plomoCard { background: rgba(14,17,22,225); border: 1px solid rgba(255,255,255,60); border-radius: 8px; }
QPushButton { background: rgba(14,17,22,195); color: #f1f3f5; border: 1px solid rgba(255,255,255,70);
              border-radius: 4px; padding: 8px 16px; font-weight: 600; }
QPushButton:hover { background: rgba(44,50,60,230); border-color: rgba(255,255,255,150); }
QPushButton:pressed { background: rgba(0,0,0,235); }
QPushButton:disabled { color: rgba(255,255,255,95); border-color: rgba(255,255,255,35); }
QPushButton#plomoPlay { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f6f6f6,stop:1 #c7cacf);
              color: #14171c; font-size: 20px; font-weight: 800; border: none;
              border-bottom: 5px solid #1b1d21; border-radius: 4px; min-height: 52px; }
QPushButton#plomoPlay:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #dadde1); }
QPushButton#plomoPlay:pressed { border-bottom: 2px solid #1b1d21; padding-top: 3px; }
QPushButton#plomoPlay:disabled { background: rgba(58,62,68,215); color: rgba(255,255,255,115);
              border-bottom: 5px solid rgba(0,0,0,190); }
QPushButton#plomoPrimary { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f6f6f6,stop:1 #c7cacf);
              color: #14171c; font-weight: 800; border: none; border-bottom: 3px solid #1b1d21; }
QPushButton#plomoPrimary:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #dadde1); }
QPushButton#plomoPrimary:disabled { background: rgba(58,62,68,215); color: rgba(255,255,255,115); }
QPushButton#plomoNotice { background: rgba(14,17,22,215); color: #ffffff; border: 1px solid rgba(255,255,255,110);
              border-radius: 4px; padding: 7px 14px; font-weight: 600; }
QPushButton#plomoNotice:hover { background: rgba(44,50,60,235); border-color: rgba(255,255,255,190); }
QPushButton#plomoLink { background: transparent; border: none; color: rgba(255,255,255,185);
              font-weight: 500; padding: 4px 2px; text-align: left; }
QPushButton#plomoLink:hover { color: #ffffff; }
QPushButton#plomoLink:disabled { color: rgba(255,255,255,80); }
QProgressBar { background: rgba(0,0,0,175); border: 1px solid rgba(255,255,255,60); border-radius: 3px;
               min-height: 12px; max-height: 12px; text-align: center; color: transparent; }
QProgressBar::chunk { background: #e8eaed; border-radius: 2px; }
QListWidget { background: rgba(0,0,0,130); color: #eceff2; border: 1px solid rgba(255,255,255,50);
              border-radius: 4px; outline: 0; font-size: 14px; }
QListWidget::item { padding: 9px 10px; border-bottom: 1px solid rgba(255,255,255,18); }
QListWidget::item:hover { background: rgba(255,255,255,18); }
QListWidget::item:selected { background: rgba(255,255,255,48); color: #ffffff; }
QSlider::groove:horizontal { height: 8px; background: rgba(255,255,255,40); border-radius: 4px; }
QSlider::sub-page:horizontal { background: #e8eaed; border-radius: 4px; }
QSlider::handle:horizontal { width: 22px; height: 22px; margin: -8px 0; border-radius: 3px;
              background: #f4f4f4; border: 2px solid #1b1d21; }
QScrollBar:vertical { background: transparent; width: 10px; }
QScrollBar::handle:vertical { background: rgba(255,255,255,70); border-radius: 4px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QMenu { background: #14171c; color: #eceff2; border: 1px solid #3a3f47; padding: 4px; }
QMenu::item { padding: 6px 22px; }
QMenu::item:selected { background: #2d333b; }
QMenu::separator { height: 1px; background: #3a3f47; margin: 4px 6px; }
QMessageBox, QDialog { background: #14171c; color: #eceff2; }
QMessageBox QLabel, QDialog QLabel { color: #eceff2; }
QCheckBox, QRadioButton { color: #eceff2; spacing: 8px; }
QLineEdit { background: #0b0d11; color: #f1f3f5; border: 1px solid rgba(255,255,255,70); border-radius: 4px; padding: 5px 8px; selection-background-color: #4a5260; }
)";
}

PlomoBackdrop::PlomoBackdrop(QWidget* parent, Mode mode) : QWidget(parent), m_art(":/plomo/background.jpg"), m_mode(mode) {}

void PlomoBackdrop::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(12, 14, 18));
    if (!m_art.isNull()) {
        const QSize scaled = m_art.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        // Anchored to the right so the soldier stays in frame when the window is narrower than 16:9.
        p.drawPixmap(QRect(QPoint(width() - scaled.width(), (height() - scaled.height()) / 2), scaled), m_art);
    }
    if (m_mode == Mode::Page) {
        p.fillRect(rect(), QColor(8, 10, 14, 205));
        return;
    }
    QLinearGradient left(0, 0, width() * 0.65, 0);
    left.setColorAt(0.0, QColor(8, 10, 14, 238));
    left.setColorAt(0.55, QColor(8, 10, 14, 150));
    left.setColorAt(1.0, QColor(8, 10, 14, 0));
    p.fillRect(rect(), left);
    QLinearGradient bottom(0, height() * 0.65, 0, height());
    bottom.setColorAt(0.0, QColor(8, 10, 14, 0));
    bottom.setColorAt(1.0, QColor(8, 10, 14, 170));
    p.fillRect(rect(), bottom);
}

PlomoPage::PlomoPage(const QString& title, const QString& subtitle, QWidget* parent) : PlomoBackdrop(parent, Mode::Page)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(40, 28, 40, 28);

    auto* back = new QPushButton(tr("←  Volver"), this);
    back->setCursor(Qt::PointingHandCursor);
    auto* top = new QHBoxLayout();
    top->addWidget(back);
    top->addStretch();
    outer->addLayout(top);
    connect(back, &QPushButton::clicked, this, &PlomoPage::backRequested);
    auto* esc = new QShortcut(Qt::Key_Escape, this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &PlomoPage::backRequested);

    auto* card = new QFrame(this);
    card->setObjectName("plomoCard");
    card->setFixedWidth(580);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(12);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("plomoTitle");
    cardLayout->addWidget(titleLabel);
    auto* subLabel = new QLabel(subtitle, card);
    subLabel->setObjectName("plomoHint");
    subLabel->setWordWrap(true);
    cardLayout->addWidget(subLabel);
    m_body = new QVBoxLayout();
    m_body->setSpacing(10);
    cardLayout->addLayout(m_body);

    auto* center = new QHBoxLayout();
    center->addStretch();
    center->addWidget(card);
    center->addStretch();
    outer->addStretch(1);
    outer->addLayout(center);
    outer->addStretch(1);
}

// ---------------------------------------------------------------- accounts

PlomoAccountsPage::PlomoAccountsPage(QWidget* parent)
    : PlomoPage(tr("Cuentas"), tr("Inicia sesión con la cuenta de Minecraft con la que vas a jugar."), parent)
{
    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(32, 32));
    m_list->setMinimumHeight(180);
    body()->addWidget(m_list);

    auto* addRow = new QHBoxLayout();
    auto* add = new QPushButton(tr("Añadir cuenta Microsoft"), this);
    add->setObjectName("plomoPrimary");
    auto* addOffline = new QPushButton(tr("Añadir cuenta offline"), this);
    addRow->addWidget(add);
    addRow->addWidget(addOffline);
    addRow->addStretch();
    body()->addLayout(addRow);

    auto* row = new QHBoxLayout();
    m_default = new QPushButton(tr("Usar esta cuenta"), this);
    m_refresh = new QPushButton(tr("Actualizar"), this);
    m_remove = new QPushButton(tr("Quitar"), this);
    for (auto* b : {add, addOffline, m_default, m_refresh, m_remove})
        b->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_default);
    row->addWidget(m_refresh);
    row->addStretch();
    row->addWidget(m_remove);
    body()->addLayout(row);

    auto accounts = APPLICATION->accounts();
    connect(add, &QPushButton::clicked, this, [this, accounts] {
        if (auto account = MSALoginDialog::newAccount(this)) {
            accounts->addAccount(account);
            if (accounts->count() == 1)
                accounts->setDefaultAccount(account);
        }
    });
    connect(addOffline, &QPushButton::clicked, this, [this, accounts] {
        // Same rule as the stock launcher: an offline profile needs a valid Microsoft account that owns the game.
        if (!accounts->anyAccountIsValid()) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Debes añadir primero una cuenta Microsoft que posea Minecraft antes de poder añadir una cuenta offline."));
            return;
        }
        ChooseOfflineNameDialog dialog(tr("Escribe el nombre de usuario de tu cuenta offline."), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (const MinecraftAccountPtr account = MinecraftAccount::createOffline(dialog.getUsername())) {
            account->login()->start();
            accounts->addAccount(account);
            if (accounts->count() == 1)
                accounts->setDefaultAccount(account);
        }
    });
    connect(m_default, &QPushButton::clicked, this, [this, accounts] {
        const int r = currentRow();
        if (r >= 0)
            accounts->setDefaultAccount(accounts->at(r));
    });
    connect(m_refresh, &QPushButton::clicked, this, [this, accounts] {
        const int r = currentRow();
        if (r >= 0)
            accounts->requestRefresh(accounts->at(r)->internalId());
    });
    connect(m_remove, &QPushButton::clicked, this, [this, accounts] {
        const int r = currentRow();
        if (r < 0)
            return;
        if (QMessageBox::question(this, tr("Quitar cuenta"), tr("¿Seguro que quieres quitar esta cuenta del launcher?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            accounts->removeAccount(accounts->index(r, 0));
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &PlomoAccountsPage::updateButtons);
    connect(m_list, &QListWidget::itemDoubleClicked, m_default, &QPushButton::click);
    connect(accounts, &AccountList::listChanged, this, &PlomoAccountsPage::refresh);
    connect(accounts, &AccountList::defaultAccountChanged, this, &PlomoAccountsPage::refresh);
    connect(accounts, &AccountList::listActivityChanged, this, &PlomoAccountsPage::refresh);
    refresh();
}

int PlomoAccountsPage::currentRow() const
{
    const int r = m_list->currentRow();
    return r >= 0 && r < APPLICATION->accounts()->count() ? r : -1;
}

void PlomoAccountsPage::updateButtons()
{
    const bool selected = currentRow() >= 0;
    m_default->setEnabled(selected);
    m_refresh->setEnabled(selected);
    m_remove->setEnabled(selected);
}

void PlomoAccountsPage::refresh()
{
    auto accounts = APPLICATION->accounts();
    const auto defaultAccount = accounts->defaultAccount();
    const int keep = m_list->currentRow();
    m_list->clear();
    for (int i = 0; i < accounts->count(); ++i) {
        const auto account = accounts->at(i);
        QString state;
        switch (account->accountState()) {
            case AccountState::Online: state = tr("Conectada"); break;
            case AccountState::Working: state = tr("Comprobando..."); break;
            case AccountState::Offline: state = tr("Sin conexión"); break;
            case AccountState::Unchecked: state = tr("Sin comprobar"); break;
            default: state = tr("Requiere iniciar sesión de nuevo"); break;
        }
        QString type = account->typeString();
        const QString name = account->profileName().isEmpty() ? account->displayName() : account->profileName();
        QString text = QString("%1\n%2  ·  %3").arg(name, type, state);
        if (defaultAccount && account->internalId() == defaultAccount->internalId())
            text = tr("%1   ★ En uso").arg(text);
        auto* item = new QListWidgetItem(text, m_list);
        const auto face = account->getFace();
        if (!face.isNull())
            item->setIcon(QIcon(face));
    }
    if (m_list->count() == 0) {
        auto* empty = new QListWidgetItem(tr("Todavía no hay ninguna cuenta. Pulsa «Añadir cuenta Microsoft»."), m_list);
        empty->setFlags(Qt::NoItemFlags);
    } else {
        m_list->setCurrentRow(std::clamp(keep, 0, m_list->count() - 1));
    }
    updateButtons();
}

// ---------------------------------------------------------------- language

PlomoLanguagePage::PlomoLanguagePage(QWidget* parent) : PlomoPage(tr("Idioma"), tr("Idioma del launcher."), parent)
{
    m_list = new QListWidget(this);
    m_list->setMinimumHeight(110);
    m_list->setMaximumHeight(110);
    body()->addWidget(m_list);

    auto* row = new QHBoxLayout();
    auto* hint = new QLabel(tr("El launcher se reiniciará para aplicar el idioma."), this);
    hint->setObjectName("plomoHint");
    m_apply = new QPushButton(tr("Aplicar"), this);
    m_apply->setObjectName("plomoPrimary");
    m_apply->setCursor(Qt::PointingHandCursor);
    row->addWidget(hint);
    row->addStretch();
    row->addWidget(m_apply);
    body()->addLayout(row);

    connect(m_list, &QListWidget::currentItemChanged, this, [this] {
        const auto* item = m_list->currentItem();
        m_apply->setEnabled(item && item->data(Qt::UserRole).toString() != APPLICATION->translations()->selectedLanguage());
    });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        const auto* item = m_list->currentItem();
        if (!item)
            return;
        APPLICATION->settings()->set("Language", item->data(Qt::UserRole).toString());
        // Every label is built once, so the simplest way to retranslate all of them is a fresh start.
        const QString exe = QCoreApplication::applicationFilePath();
#ifdef Q_OS_WIN
        QProcess restart;
        restart.setProgram("cmd.exe");
        restart.setNativeArguments(QString("/c ping -n 3 127.0.0.1 >nul & start \"\" \"%1\"").arg(QDir::toNativeSeparators(exe)));
        restart.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= 0x08000000; });
        restart.startDetached();
#else
        QProcess::startDetached(exe, {});
#endif
        QCoreApplication::quit();
    });
    refresh();
}

void PlomoLanguagePage::refresh()
{
    auto translations = APPLICATION->translations();
    const QString current = translations->selectedLanguage();
    m_list->clear();
    // The launcher only ships Spanish and English.
    for (int row = 0; row < translations->rowCount(QModelIndex()); ++row) {
        const auto index = translations->index(row, 0);
        const QString key = translations->data(index, Qt::UserRole).toString();
        if (key != "es" && key != "en_US")
            continue;
        auto* item = new QListWidgetItem(key == "es" ? QStringLiteral("Español") : QStringLiteral("English"), m_list);
        item->setData(Qt::UserRole, key);
        if (key == current)
            m_list->setCurrentItem(item);
    }
    m_apply->setEnabled(false);
}

// ------------------------------------------------------------------ memory

namespace {
constexpr int kStepMiB = 512;
constexpr int kMinMiB = 2048;
constexpr int kCapMiB = 32768;
}  // namespace

PlomoMemoryPage::PlomoMemoryPage(QWidget* parent)
    : PlomoPage(tr("Memoria"), tr("RAM que el juego puede usar. Más memoria no siempre es mejor: deja libre la que necesite Windows."),
                parent)
{
    m_value = new QLabel(this);
    m_value->setObjectName("plomoValue");
    body()->addWidget(m_value);

    const int total = static_cast<int>(HardwareInfo::totalRamMiB());
    const int maxMiB = std::max(kMinMiB, std::min(total - 2048, kCapMiB));
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(kMinMiB / kStepMiB, maxMiB / kStepMiB);
    m_slider->setCursor(Qt::PointingHandCursor);
    body()->addWidget(m_slider);

    auto* hint = new QLabel(tr("Memoria total del equipo: %1 GB").arg(total / 1024.0, 0, 'f', 1), this);
    hint->setObjectName("plomoHint");
    body()->addWidget(hint);

    auto* row = new QHBoxLayout();
    m_saved = new QLabel(this);
    m_saved->setObjectName("plomoHint");
    auto* save = new QPushButton(tr("Guardar"), this);
    save->setObjectName("plomoPrimary");
    save->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_saved);
    row->addStretch();
    row->addWidget(save);
    body()->addLayout(row);

    connect(m_slider, &QSlider::valueChanged, this, [this] {
        m_saved->clear();
        updateLabel();
    });
    connect(save, &QPushButton::clicked, this, [this] {
        auto settings = APPLICATION->settings();
        const int maxValue = m_slider->value() * kStepMiB;
        settings->set("MaxMemAlloc", maxValue);
        if (settings->get("MinMemAlloc").toInt() > maxValue)
            settings->set("MinMemAlloc", maxValue);
        m_saved->setText(tr("Guardado. Se aplicará la próxima vez que inicies el juego."));
    });
    refresh();
}

void PlomoMemoryPage::refresh()
{
    const int current = APPLICATION->settings()->get("MaxMemAlloc").toInt();
    m_slider->setValue(std::clamp(current / kStepMiB, m_slider->minimum(), m_slider->maximum()));
    m_saved->clear();
    updateLabel();
}

void PlomoMemoryPage::updateLabel()
{
    m_value->setText(tr("%1 GB").arg(m_slider->value() * kStepMiB / 1024.0, 0, 'f', 1));
}
