# Version: 0.0.1
FROM fotengauer/altlinux-sisyphus
MAINTAINER Pavel Vainerman <pv@etersoft.ru>

RUN rm -rf /etc/apt/sources.list.d/* && apt-get update

{%- if node['apt']['sources_list_filename'] %}
# project sources
COPY {{ node['apt']['sources_list_filename'] }} /etc/apt/sources.list.d/
{%- endif %}

RUN apt-get update && apt-get -y install sudo su mc openssh \
eepm apt-repo passwd app-defaults shadow-utils console-scripts xterm \ 
xorg-font-utils libxml2 setxkbmap numlockx sessreg \
openssl netcat expect zenity cups cifs-utils \
dbus-tools-gui bc curl python-base xdpyinfo iconv \
xmodmap xrdb xsetroot iproute2 lxde-lite rx-etersoft \
&& apt-get clean

{% include "base/Dockerfile.basic.tpl" %}

COPY i18n /etc/sysconfig/
COPY root.i18n /root/.i18n

# start default services
RUN service sshd start
RUN service consolesaver start

RUN useradd guest && usermod -a -G users guest && usermod -a -G lp,wheel guest && echo "guest:123" | chpasswd
RUN control su public
RUN control sudo public

{%- include "base/Dockerfile.start-command.tpl" %}
