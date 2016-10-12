#iptables -t mangle -A prerouting -p udp --dport 9999 -j TPROXY --tproxy-mark \
#    0x1/0x1 --on-port 9990

echo "1" > /proc/sys/net/ipv4/ip_forward
sudo sysctl -p

iptables -t nat -A OUTPUT -o lo -p udp --dport 8888 -j REDIRECT --to-port 8880
