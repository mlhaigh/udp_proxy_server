echo "1" > /proc/sys/net/ipv4/ip_forward
sysctl -p

ip rule add fwmark 1 lookup 100
ip route add local 0.0.0.0/0 dev lo table 100

iptables -t mangle -N DIVERT
iptables -t mangle -A PREROUTING -p udp -m socket -j DIVERT
iptables -t mangle -A DIVERT -j MARK --set-mark 1
iptables -t mangle -A DIVERT -j ACCEPT

iptables -t mangle -A PREROUTING -p udp --dport 8888 -j TPROXY --tproxy-mark \
    0x1/0x1 --on-port 8880

#iptables -t nat -A OUTPUT -o lo -p udp --dport 8888 -j REDIRECT --to-port 8880
