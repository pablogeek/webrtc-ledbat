# encoding: utf-8
import socket
import sys
import time

PORT = 4000
HOST = "localhost"
ADDR = (HOST, PORT)

def listen():
    MEASURE_INTERVAL = 0.5
    ssock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssock.bind(ADDR)
    ssock.listen(5)
    print "Listening on", ADDR
    with open("netgen.log", "w") as netgen_log:
        while True:
            sock, sender_addr = ssock.accept()
            print "Accepted connection!"
            t = time.time()
            received_bits = 0

            while True:
                try:
                    if time.time() - t >= MEASURE_INTERVAL:
                        kbits = (received_bits / MEASURE_INTERVAL) / 1000
                        netgen_log.write("%s\t%s\n" % (int(time.time() * 1000), kbits / 1000.0))
                        netgen_log.flush()
                        if kbits >= 1000:
                            print "Avg speed: %smbit/s" % (kbits / 1000.0)
                        else:
                            print "Avg speed: %skbit/s" % kbits
                        t = time.time()
                        received_bits = 0

                    res = sock.recv(1024)
                    if res:
                        received_bits += len(res) * 8
                    else:
                        break
                except Exception as e:
                    print e
                    break

def connect(BW):
    DATA = "0123456789"
    BITS = len(DATA) * 8
    packets = max(1, int((BW / BITS) / 100))
    DATA *= packets
    BITS = len(DATA) * 8
    SLEEP = (1.0 / (BW / float(BITS))) * 0.9

    print "Connecting to", ADDR
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(ADDR)
    sent_bits = 0
    t = time.time()
    while True:
        if time.time() - t >= 1:
            kbits = sent_bits / 1000
            if kbits >= 1000:
                print "Avg speed: %smbit/s" % (kbits / 1000.0)
            else:
                print "Avg speed: %skbit/s" % kbits
            t = time.time()
            sent_bits = 0

        if sent_bits >= BW:
            if SLEEP > 0.001:
                time.sleep(SLEEP)
            continue

        if sock.send(DATA):
            sent_bits += BITS
            if SLEEP > 0.001:
                time.sleep(SLEEP)
    sock.close()


if __name__ == "__main__":
    remove = []
    for n, arg in enumerate(sys.argv):
        if arg == "--port":
            PORT = int(sys.argv[n+1])
            ADDR = (HOST, PORT)
            remove.append(n)
            remove.append(n+1)
        if arg == "--host":
            HOST = sys.argv[n+1]
            ADDR = (HOST, PORT)
            remove.append(n)
            remove.append(n+1)
    for r in reversed(remove):
        del sys.argv[r]
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg.lower().endswith("kbit"):
            connect(int(arg[:-len("kbit")]) * 1000)
        elif arg.lower().endswith("mbit"):
            connect(int(arg[:-len("mbit")]) * 1000 * 1000)
        else:
            connect(int(arg))
    else:
        listen()
