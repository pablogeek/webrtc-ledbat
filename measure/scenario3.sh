rm netgen.log
rm ping.log
rm stats_logstream_ledbat.log
rm stats_logstream_sctp.log

python netgen.py &
NETGEN_PID=$!
python recordping.py &
PING_PID=$!
./rtcevent.sh 45 sctp &
sleep 15
./rtcevent.sh 45 &
sleep 10
./netgenevent.sh 10 100mbit
sleep 25
kill $NETGEN_PID
kill $PING_PID
cp ping.log scenario3/
cp netgen.log scenario3/
cp stats_logstream_sctp.log scenario3/
cp stats_logstream_ledbat.log scenario3/
cd scenario3/
./plot.sh && eog output.png

