# On arm libnalla250_soc_lib.so is copied into same direcectory as BIST exe
export LD_LIBRARY_PATH="."
loop=1
if [ $# == 1 ];then 
	loop=$1
fi
for i in `seq 1 $loop`;
do
	echo "Running $i of $loop"
	./250_soc_bist -f ../../bist_configs/250_soc_bist_config
done