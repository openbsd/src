# $OpenBSD: mm-1-setup.sh,v 1.1 1999/08/05 22:41:39 niklas Exp $
# $EOM: mm-1-setup.sh,v 1.1 1999/08/05 15:07:37 niklas Exp $

# XXX Need to start isakmpd here i n a nice way.

echo "C set [Phase 1]:127.0.0.1=localhost 1">/tmp/fifo
echo "C set [localhost]:phase=1 1">/tmp/fifo
echo "C set [localhost]:transport=udp 1">/tmp/fifo
echo "C set [localhost]:address=127.0.0.1 1">/tmp/fifo
echo "C set [localhost]:port=1501 1">/tmp/fifo
echo "C set [localhost]:configuration=default-main-mode 1">/tmp/fifo
echo "C set [localhost]:authentication=mekmitasdigoat 1">/tmp/fifo
