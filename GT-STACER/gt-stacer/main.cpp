#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include "app.h"
#include "Managers/bidi_fixer.h"
#include "Managers/alert_manager.h"
#include "Managers/setting_manager.h"
#include "Managers/update_checker.h"
#include "Dialogs/welcome_dialog.h"
#include "Dialogs/whats_new_dialog.h"
#include "../gt-stacer-core/Tools/notification_tool.h"
#include "../gt-stacer-core/Tools/relief_tool.h"
#include <QSocketNotifier>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <QFileInfo>
#include <exception>
#include <execinfo.h>
#include <fcntl.h>
#include <typeinfo>
#include <cxxabi.h>
#include <unistd.h>

namespace {
// SIGTERM / SIGINT / SIGHUP (logout, `kill`, closing the terminal) used to end
// the app without any clean-up — System Relief's frozen apps stayed frozen.
// The handler only writes a byte to a pipe; the event loop then quits normally,
// so every destructor (ReliefPage thaws what it froze) runs.
int  g_sigPipe[2] = {-1, -1};
bool g_quitRequested = false;   // a quit signal arrived (maybe inside a modal dialog's loop)
void onQuitSignal(int)
{
    const char c = 1;
    [[maybe_unused]] ssize_t n = ::write(g_sigPipe[1], &c, 1);
}
// ── Crash record (26.12) ────────────────────────────────────────────────────
// A crash used to close the app with no word at all (found in testing: a
// dangling reference in the USB tab → std::bad_alloc → abort). Now the reason
// and the call stack go to crash.log, and the next start says so.
// The handlers only use async-signal-safe calls (open/write/backtrace_symbols_fd).
char g_crashPath[4096] = {0};

void writeCrash(const char *what)
{
    if (!g_crashPath[0]) return;
    const int fd = ::open(g_crashPath, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return;
    const char head[] = "GT-STACER " APP_VERSION " crashed: ";
    [[maybe_unused]] ssize_t n = ::write(fd, head, sizeof(head) - 1);
    n = ::write(fd, what, std::strlen(what));
    n = ::write(fd, "\n", 1);
    void *frames[64];
    const int count = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, count, fd);
    n = ::write(fd, "\n", 1);
    ::close(fd);
}

void onCrashSignal(int sig)
{
    const char *what = sig == SIGSEGV ? "SIGSEGV (invalid memory access)"
                     : sig == SIGBUS  ? "SIGBUS (bus error)"
                     : sig == SIGFPE  ? "SIGFPE (arithmetic error)"
                     : sig == SIGILL  ? "SIGILL (illegal instruction)"
                     :                  "SIGABRT (aborted)";
    writeCrash(what);
    std::signal(sig, SIG_DFL);      // then die as before (core dump, exit status)
    ::raise(sig);
}

void onTerminate()
{
    // An exception nobody caught: name it before aborting.
    static char buf[512];
    const char *type = "unknown exception";
    if (std::type_info *t = abi::__cxa_current_exception_type()) type = t->name();
    std::snprintf(buf, sizeof buf, "uncaught C++ exception (%s)", type);
    try { if (auto e = std::current_exception()) std::rethrow_exception(e); }
    catch (const std::exception &e) { std::snprintf(buf, sizeof buf, "uncaught C++ exception (%s): %s", type, e.what()); }
    catch (...) {}
    writeCrash(buf);
    std::signal(SIGABRT, SIG_DFL);  // don't record the abort a second time
    std::abort();
}

// Returns the path of a crash record left by the previous run ("" = none),
// renamed so it is reported once; and arms the handlers for this run.
QString installCrashRecord()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/crash.log";
    QString previous;
    if (QFileInfo(path).size() > 0) {
        previous = dir + "/crash-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".log";
        if (!QFile::rename(path, previous)) previous = path;
    }
    std::strncpy(g_crashPath, QFile::encodeName(path).constData(), sizeof g_crashPath - 1);
    std::set_terminate(onTerminate);
    for (int sig : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT}) std::signal(sig, onCrashSignal);
    return previous;
}

void installQuitSignals(QCoreApplication *app)
{
    if (::pipe(g_sigPipe) != 0) return;
    auto *notifier = new QSocketNotifier(g_sigPipe[0], QSocketNotifier::Read, app);
    QObject::connect(notifier, &QSocketNotifier::activated, app, [] {
        char c;
        [[maybe_unused]] ssize_t n = ::read(g_sigPipe[0], &c, 1);
        g_quitRequested = true;
        // exit(), not quit(): in Qt 6 quit() first asks every window to close,
        // and our main window REFUSES (minimize-to-tray), which cancels the quit —
        // verified: SIGTERM was handled and the app kept running. exit() also
        // ends a modal dialog's exec() loop.
        QCoreApplication::exit(0);
    });
    struct sigaction sa {};
    sa.sa_handler = onQuitSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : {SIGTERM, SIGINT, SIGHUP}) sigaction(sig, &sa, nullptr);
}
} // namespace

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

    // A GUI launched from a desktop entry often inherits a PATH without the sbin
    // dirs, so admin tools like `ufw` (in /usr/sbin, /sbin) look "missing" to
    // QStandardPaths::findExecutable — which made the Firewall page report "no
    // supported firewall" even when ufw/firewalld was installed. Ensure the
    // standard system dirs are on PATH before any tool probing. (No-op inside
    // Flatpak, where detection goes through `flatpak-spawn --host`.)
    {
        QList<QByteArray> parts = qgetenv("PATH").split(':');
        bool changed = false;
        for (const QByteArray &dir : { QByteArrayLiteral("/usr/local/sbin"),
                                       QByteArrayLiteral("/usr/sbin"),
                                       QByteArrayLiteral("/sbin") }) {
            if (!parts.contains(dir)) { parts.append(dir); changed = true; }
        }
        if (changed) qputenv("PATH", parts.join(':'));
    }
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
    installQuitSignals(&app);
    const QString previousCrash = installCrashRecord();
    // An earlier GT-STACER that was killed (SIGKILL, crash, power loss of the
    // session) may have left apps frozen by System Relief — thaw them first,
    // before this instance's Relief page writes its own journal.
    const int recoveredFrozen = ReliefTool::recoverFrozenJournal();
    // Arabic text with commands/paths, fixed for every widget of the app.
    BidiFixer::install();

    // Screenshot capture mode (website assets):
    //   gt-stacer --capture <dir> [--theme dark|light] [--lang en|ar] [--page N] [--tab N]
    // Set theme/language BEFORE constructing App so its startup sequence renders
    // in the requested combination, then grab every page and quit.
    QString captureDir, capTheme, capLang;
    int capPage = -1, capTab = -1;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if      (a == "--capture" && i + 1 < argc) captureDir = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--theme"   && i + 1 < argc) capTheme   = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--lang"    && i + 1 < argc) capLang    = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--page"    && i + 1 < argc) capPage    = QString::fromLocal8Bit(argv[++i]).toInt();
        else if (a == "--tab"     && i + 1 < argc) capTab     = QString::fromLocal8Bit(argv[++i]).toInt();
    }
    if (!capTheme.isEmpty()) SettingManager::instance()->setTheme(capTheme);
    if (!capLang.isEmpty())  SettingManager::instance()->setLanguage(capLang);

    App window;
    AlertManager::instance(); // starts polling once SettingManager is ready
    if (!previousCrash.isEmpty())
        NotificationTool::notify(QCoreApplication::translate("main", "GT-STACER"),
            QCoreApplication::translate("main",
                "GT-STACER closed unexpectedly last time. What happened was saved to:\n%1\n"
                "Please send this file with your bug report.").arg(previousCrash),
            NotificationTool::Urgency::Normal, "gt-stacer");
    if (recoveredFrozen > 0)
        NotificationTool::notify(QCoreApplication::translate("main", "System Relief"),
            QCoreApplication::translate("main",
                "Resumed %1 app process(es) that a previous GT-STACER had frozen and could not "
                "thaw because it was closed abruptly.").arg(recoveredFrozen),
            NotificationTool::Urgency::Normal, "gt-stacer");

    if (!captureDir.isEmpty()) {
        window.captureAllPages(captureDir, capPage, capTab);
        return app.exec();
    }

    bool startHidden = (argc >= 2 && QString(argv[1]) == "--hide");
    auto *settings = SettingManager::instance();
    if (!startHidden) {
        window.show();
        if (WelcomeDialog::shouldShow()) {
            // First run — the onboarding tour (it marks itself shown). Record the
            // version so we don't also pop the "what's new" dialog this launch.
            WelcomeDialog welcome(&window);
            welcome.exec();
            settings->setLastSeenVersion(APP_VERSION);
        } else if (settings->lastSeenVersion() != QString(APP_VERSION)) {
            // Updated since the last run — show what changed, once.
            WhatsNewDialog whatsNew(&window);
            whatsNew.exec();
            settings->setLastSeenVersion(APP_VERSION);
        }
    }

    // Opt-in background update check: ask GitHub's public API whether a newer
    // stable release exists, and notify if so. Nothing is downloaded.
    if (settings->checkUpdatesOnStartup()) {
        auto *uc = new UpdateChecker(&window);
        QObject::connect(uc, &UpdateChecker::updateAvailable, &window,
                         [](const QString &v, const QString &url) {
            NotificationTool::notify(
                QObject::tr("GT-STACER %1 is available").arg(v),
                QObject::tr("A newer version is out. Open Settings → Check now, or visit:\n%1").arg(url),
                NotificationTool::Urgency::Normal, "gt-stacer");
        });
        uc->checkNow();
    }

    // A quit signal during the welcome / what's-new dialog only ended that
    // dialog's loop — don't start the main loop again, return (destructors run).
    if (g_quitRequested) return 0;
    return app.exec();
}
