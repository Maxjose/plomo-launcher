// SPDX-License-Identifier: GPL-3.0-only
// PLOMO's own screens (home backdrop, accounts, language, memory). They replace the stock settings dialog.
#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSlider;
class QVBoxLayout;

QString plomoStyleSheet();

// Installs the built-in English strings when `languageKey` is an English locale (the source strings are Spanish).
void plomoInstallLanguage(const QString& languageKey);

// Cover art scaled to fill the widget. Home darkens the left side for the controls; Page dims it evenly.
class PlomoBackdrop : public QWidget {
    Q_OBJECT
   public:
    enum class Mode { Home, Page };
    PlomoBackdrop(QWidget* parent, Mode mode);

   protected:
    void paintEvent(QPaintEvent*) override;

   private:
    QPixmap m_art;
    Mode m_mode;
};

// Backdrop + "back" button + centred card with a title. Subclasses fill body().
class PlomoPage : public PlomoBackdrop {
    Q_OBJECT
   public:
    PlomoPage(const QString& title, const QString& subtitle, QWidget* parent);
    virtual void refresh() {}

   signals:
    void backRequested();

   protected:
    QVBoxLayout* body() const { return m_body; }

   private:
    QVBoxLayout* m_body;
};

// Full-window screen shown while the launcher updates itself: nothing else can be clicked until the installer takes over.
class PlomoUpdateScreen : public PlomoBackdrop {
    Q_OBJECT
   public:
    explicit PlomoUpdateScreen(QWidget* parent);
    void setStatus(const QString& text);
    void setProgress(int percent);  // negative: size unknown, shows a busy bar

   private:
    QLabel* m_status;
    QProgressBar* m_bar;
};

class PlomoAccountsPage : public PlomoPage {
    Q_OBJECT
   public:
    explicit PlomoAccountsPage(QWidget* parent);
    void refresh() override;

   private:
    void updateButtons();
    int currentRow() const;
    QListWidget* m_list;
    QPushButton* m_default;
    QPushButton* m_remove;
    QPushButton* m_refresh;
};

class PlomoLanguagePage : public PlomoPage {
    Q_OBJECT
   public:
    explicit PlomoLanguagePage(QWidget* parent);
    void refresh() override;

   private:
    QListWidget* m_list;
    QPushButton* m_apply;
};

class PlomoMemoryPage : public PlomoPage {
    Q_OBJECT
   public:
    explicit PlomoMemoryPage(QWidget* parent);
    void refresh() override;

   private:
    void updateLabel();
    QSlider* m_slider;
    QLabel* m_value;
    QLabel* m_saved;
};
