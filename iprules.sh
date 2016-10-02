iptables -t mangle -A prerouting -p udp --dport 9999 -j TPROXY --tproxy-mark \
    0x1/0x1 --on-port 9990

#iptables -t nat -A OUTPUT -o lo -p udp --dport 9999 -j REDIRECT --to-port 9990
