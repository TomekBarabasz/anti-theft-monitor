import socket,argparse,struct,threading,sys

CMD_START_UDP_MONITOR = 15
CMD_STOP_UDP_MONITOR  = 16
CMD_ECHO = 17
CMD_START_GPSR_CONTROLLER = 18

def set_keepalive(sock):
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    # Linux specific: after 10 idle minutes, start sending keepalives every 5 minutes. 
    # Drop connection after 10 failed keepalives
    if hasattr(socket, "TCP_KEEPIDLE") and hasattr(socket, "TCP_KEEPINTVL") and hasattr(socket, "TCP_KEEPCNT"):
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE,  60)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 60)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT,   2) 

def echo_main(Args):
    host,port = Args.tcp.split(':')
    port = int(port)
    print(f'connecting {host} port {port} ...')    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    set_keepalive(sock)
    
    udp_host,udp_port = Args.udp.split(':')
    udp_port = int(udp_port)
    udp_host_ip = [int(x) for x in udp_host.split('.')]
    tosend = struct.pack("<BBBBBH",CMD_START_UDP_MONITOR,*udp_host_ip,udp_port)
    sent = sock.send(tosend)
    assert(sent > 0)
    # Step 1: Create the UDP socket
    
    def echo_rcv(port):
        udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_socket.bind(('192.168.1.9', port))
        while True:
            data, client_address = udp_socket.recvfrom(256)
            msg = data.decode()
            print(f'rcv {msg} from {client_address}')
            if msg == 'stop':
                break
        print('exiting rcv ...')
                
    t = threading.Thread(target = echo_rcv,args = (udp_port,))
    t.start()
    while True:
        p = input('>')
        d = p.encode() #+ b'\x00'
        tosend = struct.pack("<BH",CMD_ECHO,len(d)) + d
        sock.send(tosend)
        if p == 'stop':
            break
    print('exiting input')
    t.join()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Antitheft monitor controller")
    parser.add_argument("-tcp",type = str, default="192.168.1.11:1234", help="tcp server address:port")
    parser.add_argument("-udp",type = str, default="192.168.1.9:5000", help="udp monitor address:port")
    
    Cmds = {'echo' : echo_main}
    cmd = sys.argv[1] if len(sys.argv) > 1 else None
    if cmd not in Cmds.keys():
        print('invalid command, try one of :', ','.join(Cmds.keys()))
        exit(1)
    
    Args = parser.parse_args(sys.argv[2:])
    Cmds[cmd](Args)
