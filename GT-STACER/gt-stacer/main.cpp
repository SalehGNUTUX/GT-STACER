#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include "app.h"
#include "Managers/alert_manager.h"
#include "Dialogs/welcome_dialog.h"

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    if (type == QtWarningMsg) return;

    static QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                             + "/gt-stacer.log";
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));

    QFile file(logPath);
    // Truncate if > 1 MB
    QIODevice::OpenMode mode = (file.exists() && file.size() > 1024 * 1024)
                                ? QIODevice::WriteOnly | QIODevice::Truncate
                                : QIODevice::WriteOnly | QIODevice::Append;

    if (file.open(mode)) {
        QTextStream out(&file);
        QString level;
        switch (type) {
            case QtDebugMsg:    level = "DEBUG";    break;
            case QtInfoMsg:     level = "INFO";     break;
            case QtCriticalMsg: level = "CRITICAL"; break;
            case QtFatalMsg:    level = "FATAL";    break;
            default:            level = "OTHER";    break;
        }
        out << QString("[%1] [%2] %3\n")
               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
               .arg(level)
               .arg(msg);
    }
}

int main(int argc, char *argv[])
{
    // Enable HiDPI (Qt6 enables it by default, but explicit for clarity)
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false); // البرنامج يبقى في شريط المهام عند إغلاق النافذة
    app.setApplicationName("gt-stacer");
    app.setApplicationDisplayName("GT-STACER");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("GNUTUX");
    app.setOrganizationDomain("gnutux.org");
    // Wayland (and modern GNOME/KDE) match window→desktop-entry via this name,
    // which selects both the taskbar icon and the displayed application label.
    // Without it the compositor falls back to the generic Wayland glyph ("W").
    QGuiApplication::setDesktopFileName("gt-stacer");
    // استخدام أيقونة النظام أولاً (للتكامل مع بيئة سطح المكتب في شريط المهام)
    QIcon appIcon = QIcon::fromTheme("gt-stacer",
                        QIcon(":/static/icons/gt-stacer.png"));
    app.setWindowIcon(appIcon);

    qInstallMessageHandler(messageHandler);

    App window;
    AlertManager::instance(); // starts polling once SettingManager is ready

    bool startHidden = (argc >= 2 && QString(argv[1]) == "--hide");
    if (!startHidden) {
        window.show();
        // Show onboarding on first run
        if (WelcomeDialog::shouldShow()) {
            WelcomeDialog welcome(&window);
            welcome.exec();
        }
    }

    return app.exec();
}
