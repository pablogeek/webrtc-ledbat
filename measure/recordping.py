# encoding: utf-8
import subprocess
import sys
import time
HOST = "localhost"

if len(sys.argv) > 1:
    HOST = sys.argv[1]
p = subprocess.Popen(args=["ping", "-i", "0.5", HOST], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
with open("ping.log", "w") as ping_log:
    while 1:
        retcode = p.poll()  # returns None while subprocess is running
        line = p.stdout.readline()
        if line.split()[-1].strip() == "ms":
            ping = line.split()[-2].split("=")[1]
            ping_log.write("%s\t%s\n" % (int(time.time() * 1000), ping))
            ping_log.flush()
        print line
        if retcode is not None:
            break