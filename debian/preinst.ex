#! /bin/sh
# preinst script for lamerpad
#
# see: dh_installdeb(1)

set -e

# summary of how this script can be called:
#        * <new-preinst> `install'
#        * <new-preinst> `install' <old-version>
#        * <new-preinst> `upgrade' <old-version>
#        * <old-preinst> `abort-upgrade' <new-version>

case "$1" in
    install|upgrade)
#        if [ "$1" = "upgrade" ]
#        then
#            start-stop-daemon --stop --quiet --oknodo  \
#                --pidfile /var/run/lamerpad.pid  \
#                --exec /usr/sbin/lamerpad 2>/dev/null || true
#        fi
    ;;

    abort-upgrade)
    ;;

    *)
        echo "preinst called with unknown argument \`$1'" >&2
        exit 0
    ;;
esac

# dh_installdeb will replace this with shell code automatically
# generated by other debhelper scripts.

#DEBHELPER#

exit 0


