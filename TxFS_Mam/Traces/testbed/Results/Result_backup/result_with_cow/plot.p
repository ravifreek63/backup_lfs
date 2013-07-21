set term postscript
set output "printme_without.ps"
set xlabel "Time (steps)"
set ylabel "Data (bytes)"
set xr [0:2.366e+07]
#set yr [0:325]
plot "/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/Results/Result_backup/result_without_cow/graph258.txt" using 2:1 title 'without_cow' with linespoints  lc rgb "black"
