#include "PulsePortChecker.h"

bool PulsePortChecker::isPortBusy(int port){
#ifndef __WIN32
    struct sockaddr_in address;
    int socketdesc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(socketdesc, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
    //    wxLogInfo(wxT("Failed to bind socket on port %s"), port);
        return true;
    }

    //wxLogInfo(wxT("Port %s is free"), port);
    return false;
#else
    return false;
#endif
}
