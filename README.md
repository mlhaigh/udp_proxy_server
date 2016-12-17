Columbia University / Matthew Haigh
UDP Proxy
Written in collaboration with Kevin Yang 
For Professor Dan Rubenstein and Professor Vishal Misra
For the Wifi QOS Research Project

Purpose: Proxy for UDP packets for rate control

To compile type make

Configuration file (currently proxy.conf) formatted as follows:
    each line contains an entry
    each line consists of <ip_address> <port> <rate> delimited by one space each
Enter each host:port and the rate in the configuration file
Default rates are defined in UDPProxy.h ("#define DEFAULT_RATE <rate>")

Configure iptables rules use iprules.sh
Modify/add entries for tproxy to intercept packets from the applicable source IPs
iptables can be flushed with ./clear_iprules.sh

Run as follows:
./UDPProxy <port> <log_file> <config_file>

Proxy will read the rate specifications from the <config_file>.
If logging is turned on, log statements will be written to <log>
    Logging is turned on if UDPProxy.h contains the statement: 
        "#define PRINT_LOG"

UDPClient and UDPServer are included for testing, but iperf works best
To run test: 
    1. edit proxy.conf to match your client system
        <client_ip> 2001 1
        <client_ip> 2010 10
    2. edit iprules.sh to match your system
        "iptables -t mangle -A PREROUTING -p udp -s <client_ip> -j TPROXY \
            --tproxy-mark 0x1/0x1 --on-port 7777"
        "sudo ./iprules.sh"
    3. start iperf servers on the same machine as the proxy
        "iperf -su -i 1 -p 2001 &"
        "iperf -su -i 1 -p 2010"
    4. start proxy on server machine
        "sudo ./UDPProxy 7777 log proxy.conf"
    5. start iperf clients on the client machine
        "iperf -c <server_ip> -u -t 100 -i 1 -b 1000MB -p 2001 > /tmp/log1 &"
        "iperf -c <server_ip> -u -t 100 -i 1 -b 1000MB > -p 2010 /tmp/log2 "\
    6. confirm that the rates displayed by iperf match the rates in the config
        file for the corresponding ports
    7. remember to close background processes
        "pkill iperf"

note: if packets are not received properly, you may have to run 
./cleariprules.sh on the client machine. This will remove any iptables rules 
that may interfere.

Additionally, testing may be performed using UDPClient and UDPServer 
included with UDPProxy. Simply prepare the proxy.conf and iprules in the same
manner. Run the proxy as before:
    sudo ./UDPProxy 7777 log proxy.conf
On the server machine run:
    sudo ./UDPServer 6666
On the client run:
    sudo ./UDPClient <server_ip> 6666 5555 1000
This will send 1000 packets to the server and the server will echo them back
