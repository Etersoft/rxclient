#include <iostream>
#ifndef __WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <string.h>
#include <wx/log.h>
#include <wx/string.h>
#include "trace.h"

class PulsePortChecker
{
public:
    static bool isPortBusy(int port);
};
