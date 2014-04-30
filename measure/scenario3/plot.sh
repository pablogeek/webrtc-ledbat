OFFSET=$(head -n 1 ping.log | cut -f 1)
TC="((\$1 - $OFFSET) / 1000)"
gnuplot -p <<- EOF
    set ylabel 'Traffic (Mbit)' tc lt 1
    set yrange[0:11]

    set y2label 'RTT (ms)' tc lt 4
    set y2range[0:200]
    set y2tics 50 nomirror tc lt 4
    
    set xlabel 'Time (seconds)'

    set terminal png size 800,600 enhanced
    set output 'scenario3.png'
    
    plot "netgen.log" u $TC:2 w lines t 'TCP', "stats_logstream_ledbat.log" u $TC:2 w lines t 'LEDBAT', "stats_logstream_sctp.log" u $TC:2 w lines t 'SCTP', "ping.log" u $TC:2 w l axes x1y2 t 'Ping'
EOF

cp scenario3.png ../
