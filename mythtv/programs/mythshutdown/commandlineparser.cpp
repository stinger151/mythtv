
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythShutdownCommandLineParser::MythShutdownCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHSHUTDOWN)
{ LoadArguments(); }

void MythShutdownCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addVerbose();
    addLogging();

    add(QStringList( QStringList() << "-w" << "--setwakeup" ), "setwakeup", "",
            "Set the wakeup time (yyyy-MM-ddThh:mm:ss)", "");
    add(QStringList( QStringList() << "-t" << "--setscheduledwakeup" ), "setschedwakeup",
            "Set wakeup time to the next scheduled recording", "");
    add(QStringList( QStringList() << "-q" << "--shutdown" ), "shutdown",
            "Apply wakeup time to nvram and shutdown.", "");
    add(QStringList( QStringList() << "-x" << "--safeshutdown" ), "safeshutdown",
            "Check if shutdown is possible, and shutdown", "");
    add(QStringList( QStringList() << "-p" << "--startup" ), "startup",
            "Check startup status",
            "Check startup status\n"
            "   returns 0 - automatic startup\n"
            "           1 - manual startup");
    add(QStringList( QStringList() << "-c" << "--check" ), "check", 1,
            "Check whether shutdown is possible",
            "Check whether shutdown is possible depending on input\n"
            "   input 0 - dont check recording status\n"
            "         1 - do check recording status\n\n"
            " returns 0 - ok to shut down\n"
            "         1 - not ok, idle check reset");
    add(QStringList( QStringList() << "-l" << "--lock" ), "lock",
            "disable shutdown", "");
    add(QStringList( QStringList() << "-u" << "--unlock" ), "unlock",
            "enable shutdown", "");
    add(QStringList( QStringList() << "-s" << "--status" ), "status", 1,
            "check current status",
            "check current status depending on input\n"
            "   input 0 - dont check recording status\n"
            "         1 - do check recording status\n\n"
            " returns 0 - Idle\n"
            "         1 - Transcoding\n"
            "         2 - Commercial Detection\n"
            "         4 - Grabbing EPG data\n"
            "         8 - Recording (only valid if input=1)\n"
            "        16 - Locked\n"
            "        32 - Jobs running or pending\n"
            "        64 - In daily wakeup/shutdown period\n"
            "       128 - Less than 15 minutes to next wakeup period\n"
            "       255 - Setup is running");
}
