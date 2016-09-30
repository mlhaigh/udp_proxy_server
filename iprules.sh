iptables -t nat -A OUTPUT -o lo -o udp --dport 9999 -j REDIRECT --to-port 9990
