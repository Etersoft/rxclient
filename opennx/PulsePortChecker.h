#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <wx/log.h>
#include <wx/string.h>
#include "trace.h"

class PulsePortChecker
{
public:
    static bool isPortBusy(int port);
};
