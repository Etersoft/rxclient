#!/bin/sh

update_chrooted conf

serv sshd start
serv rx-etersoft start

ip a

tail -f /var/log/nxserver.log

/bin/bash
