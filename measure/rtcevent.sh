if [ -z "$1" ]; then
    echo "Enter a runtime in seconds!"
    exit 1
fi
PORT="8888"

PROTOCOL="ledbat"
if [ -n "$2" ]; then
    PROTOCOL=$2
    PORT="9999"
fi
./server --port $PORT &
SERVER_PID=$!
./recv $PROTOCOL out $PORT &
RECV_PID=$!
./send $PROTOCOL d/garbage $PORT &
SEND_PID=$!
echo "Event: Started!"
sleep $1
pkill -P $SEND_PID
pkill -P $RECV_PID
pkill -P $SERVER_PID
echo "Event: Done!"
