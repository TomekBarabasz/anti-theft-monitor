import socket,argparse,struct,threading

def set_keepalive(sock):
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    # Linux specific: after 10 idle minutes, start sending keepalives every 5 minutes. 
    # Drop connection after 10 failed keepalives
    if hasattr(socket, "TCP_KEEPIDLE") and hasattr(socket, "TCP_KEEPINTVL") and hasattr(socket, "TCP_KEEPCNT"):
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE,  60)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 60)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT,   2) 

def test_event_monitor(sock):
    port = 5000
    tosend = struct.pack("<BBBBBH",15,192,168,1,9,port)
    send = sock.send(tosend)
    # Step 1: Create the UDP socket
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(('127.0.0.1', port))
    def echo_rcv(udp_socket):
        while True:
            data, client_address = udp_socket.recvfrom(256)
            print('rcv', data)
            if data == 'stop':
                break
                
    t = threading.Thread(target = echo_rcv,args = (udp_socket,))
    t.start()
    while True:
        p = input('>')
        d = p.encode() + b'\x00'
        tosend = struct.pack("<BH",16,len(d)) + d
        sock.send(tosend)
        if p == 'stop':
            break
    t.join()

def main(Args):
    host,port = Args.addr.split(':')
    port = int(port)
    print(f'connecting {host} port {port} ...')    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    set_keepalive(sock)
    test_event_monitor(sock)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Antitheft monitor controller")
    parser.add_argument("-addr","-a", type = str, default="192.168.1.11:1234", help="monitor tcp address:port")
    Args = parser.parse_args()
    main(Args)
