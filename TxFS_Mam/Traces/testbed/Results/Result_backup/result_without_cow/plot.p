set term postscript
set output "printme.ps"
set xlabel "Time (steps)"
set ylabel "Data (bytes)"
set xr [100:150]
#set yr [0:325]
plot "/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/Results/Result_backup/result_without_cow/graph258.txt" using 2:1 title '258' with linespoints  lc rgb "black", \
            "/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/Results/Result_backup/result_without_cow/graph291.txt" using 2:1 title '291' with linespoints  lc rgb "black", \
	    "/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/Results/Result_backup/result_without_cow/graph340.txt" using 2:1 title '340' with linespoints  lc rgb "black"
