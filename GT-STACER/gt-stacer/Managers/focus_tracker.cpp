#include "focus_tracker.h"
#include "../../gt-stacer-core/Utils/command_util.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
const char *kScriptName = "gt-stacer-focus";
const char *kPath       = "/FocusTracker";

// Plasma 6 names (activeWindow / windowActivated) with a Plasma 5 fallback
// (activeClient / clientActivated). %1 = our D-Bus unique name.
const char *kScript = R"JS(
var gtTarget = "%1";
function gtReport(w) {
    if (w && w.pid)
        callDBus(gtTarget, "/FocusTracker", "org.gnutux.gtstacer.FocusTracker",
                 "activated", w.pid, String(w.resourceClass));
}
if (workspace.windowActivated !== undefined) {
    workspace.windowActivated.connect(gtReport);
    gtReport(workspace.activeWindow);
} else {
    workspace.clientActivated.connect(gtReport);
    gtReport(workspace.activeClient);
}
)JS";
} // namespace

FocusTracker *FocusTracker::instance()
{
    static FocusTracker *s = new FocusTracker(qApp);
    return s;
}

FocusTracker::FocusTracker(QObject *parent) : QObject(parent) {}

FocusTracker::~FocusTracker() { stopKWin(); }

void FocusTracker::acquire()
{
    if (m_refs++ > 0) return;
    if (startKWin()) { m_backend = Backend::KWin; return; }
    const bool x11 = QGuiApplication::platformName() == "xcb"
                  || qEnvironmentVariable("XDG_SESSION_TYPE") == "x11";
    m_backend = (x11 && CommandUtil::commandExists("xprop")) ? Backend::Xprop : Backend::None;
}

void FocusTracker::release()
{
    if (m_refs == 0 || --m_refs > 0) return;
    stopKWin();
    m_backend = Backend::None;
    m_pid = 0;
}

int FocusTracker::focusedPid()
{
    if (m_backend != Backend::Xprop) return m_pid;
    // _NET_ACTIVE_WINDOW(WINDOW): window id # 0x3a00007
    const QString root = CommandUtil::execProgramC("xprop", {"-root", "_NET_ACTIVE_WINDOW"}, 2000);
    const auto w = QRegularExpression("#\\s*(0x[0-9a-fA-F]+)").match(root);
    if (!w.hasMatch() || w.captured(1) == "0x0") return 0;
    const QString win = CommandUtil::execProgramC("xprop", {"-id", w.captured(1), "_NET_WM_PID"}, 2000);
    const auto p = QRegularExpression("=\\s*(\\d+)").match(win);
    const int pid = p.hasMatch() ? p.captured(1).toInt() : 0;
    if (pid != m_pid) { m_pid = pid; if (pid > 0) emit focusChanged(pid); }
    return m_pid;
}

void FocusTracker::activated(int pid, const QString &)
{
    if (pid <= 0 || pid == m_pid) return;
    m_pid = pid;
    emit focusChanged(pid);
}

bool FocusTracker::startKWin()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()
        || !bus.interface()->isServiceRegistered("org.kde.KWin").value())
        return false;

    if (!m_registered)
        m_registered = bus.registerObject(kPath, this, QDBusConnection::ExportScriptableSlots);
    if (!m_registered) return false;

    // The script file must be readable by KWin (on the host): the cache dir is
    // a real host path in every packaging (Flatpak maps it under ~/.var/app).
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/kwin-focus.js";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QString::fromLatin1(kScript).arg(bus.baseService()).toUtf8());
    f.close();

    QDBusInterface scripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", bus);
    if (!scripting.isValid()) return false;
    // A previous run that crashed may have left the script loaded.
    scripting.call("unloadScript", QString::fromLatin1(kScriptName));
    const QDBusReply<int> id = scripting.call("loadScript", path, QString::fromLatin1(kScriptName));
    if (!id.isValid() || id.value() < 0) return false;

    // Plasma 6: /Scripting/Script<N>; Plasma 5: /<N>.
    for (const QString &obj : {QString("/Scripting/Script%1").arg(id.value()),
                               QString("/%1").arg(id.value())}) {
        QDBusInterface script("org.kde.KWin", obj, "org.kde.kwin.Script", bus);
        if (script.isValid() && script.call("run").type() != QDBusMessage::ErrorMessage)
            return true;
    }
    scripting.call("unloadScript", QString::fromLatin1(kScriptName));
    return false;
}

void FocusTracker::stopKWin()
{
    if (m_backend != Backend::KWin) return;
    QDBusInterface scripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                             QDBusConnection::sessionBus());
    if (scripting.isValid()) scripting.call("unloadScript", QString::fromLatin1(kScriptName));
}
