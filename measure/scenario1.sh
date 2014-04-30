rm netgen.log
rm ping.log
rm stats_logstream_ledbat.log
rm stats_logstream_sctp.log

python netgen.py &
NETGEN_PID=$!
python recordping.py &
PING_PID=$!
./rtcevent.sh 30 &
sleep 10
./netgenevent.sh 5 5mbit &
sleep 5
./netgenevent.sh 5 100mbit
sleep 10
kill $NETGEN_PID
kill $PING_PID
mkdir -p scenario1
cp ping.log scenario1/
cp netgen.log scenario1/
cp stats_logstream_ledbat.log scenario1/
cd scenario1/
./plot.sh && eog scenario1.png
