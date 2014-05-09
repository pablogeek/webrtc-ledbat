# encoding: utf-8
import sys
args = sys.argv[1:]
if len(args) < 2:
    print u"Usage: RESULTFILE FILE..."
    sys.exit(1)

seconds = {}
for filename in args[1:]:
    with open(filename, "r") as f:
        for line in f:
            time, value = line.strip().split()
            second = int(time) / 1000
            if second not in seconds:
                seconds[second] = {}
            if filename not in seconds[second]:
                seconds[second][filename] = []
            seconds[second][filename].append(float(value))

with open(args[0], "w") as out:
    for second, entry in sorted(seconds.items(), key=lambda x: x[0]):
        total = 0
        for filename, values in entry.items():
            total += sum(values)
        out.write("%s\t%s\n" % (second * 1000, total))