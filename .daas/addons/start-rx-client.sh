#!/bin/sh

update_chrooted conf

control su public

serv sshd start
serv freenx-server start
serv cups start

ip a

tail -f /var/log/nxserver.log

/bin/bash
