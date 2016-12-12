iptables -F
iptables -t mangle -F

ip rule add fwmark 1 lookup 100
ip route add local 0.0.0.0/0 dev lo table 100
iptables -t mangle -A PREROUTING -p udp --sport 7777 -j RETURN
iptables -t mangle -A PREROUTING -p udp -s 160.39.135.160 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 7777
