if [ -z "$1" ]; then
    echo "Enter a runtime in seconds!"
    exit 1
fi
if [ -z "$2" ]; then
    echo "Enter a speed in m/k-bit!"
    exit 1
fi

#if [ $3!="noserver"]; then
#    python netgen.py &
#    SERVER_PID=$!
#fi
python netgen.py $2 &
SEND_PID=$!
echo "Event: Started!"
sleep $1
kill $SEND_PID
#if [ -n "$SERVER_PID" ]; then
#    kill $SERVER_PID
#fi
echo "Event: Done!"
