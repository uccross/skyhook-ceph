#!/bin/bash

nosds=$1
num_objs=$2
poolname=$3
data_path=$4
schema_path=$5
actually_do_it=$6
number_of_groups=$7
fbwriter_filename="fbmeta.Skyhook.v2.SFT_FLATBUF_FLEX_ROW.ncols100_paper_exps.0.1-1"
worker_threads=24
queue_depth=24
format="arrow"
DATA_SCHEMA="\"0 8 1 0 ATT0;\""


# make the pool
rados mkpool $poolname ;

# make bins
sudo make -j36 fbwriter run-query run-copyfrom-merge ;

# write the data
for ((j=0; j<${nosds}; j++)); do
  echo "clearing cache on osd"$j
  ssh osd${j} sync
  ssh osd${j} "echo 1 | sudo tee /proc/sys/vm/drop_caches"
  ssh osd${j} sync
done
start=$(date --utc "+%s.%N")
for ((group_id=0; group_id<${number_of_groups}; group_id++)); do

  # write the csv to fbxrows
  cmd0="sudo bin/fbwriter --file_name $data_path --schema_file_name  $schema_path --num_objs 1 --flush_rows 25000 --read_rows 25000 --csv_delim \"|\" --use_hashing false --rid_start_value 1 --table_name ncols100_paper_exps --input_oid 0 --obj_type SFT_FLATBUF_FLEX_ROW ;"
  eval "$cmd0"

  # write the fbxrows from disk to ceph
  cmd1="sudo python rados_put_parallel.py $num_objs $poolname ./$fbwriter_filename $actually_do_it $group_id"
  eval "$cmd1"

  # remove the fbxrows disk file to conserve space
  cmd2="sudo rm $fbwriter_filename"
  eval "$cmd2"

  # transform the fbxrows into arrow
  cmd3="sudo bin/run-query --num-objs ${num_objs} --pool ${poolname} --wthreads ${worker_threads} --qdepth ${queue_depth}  --transform-db --transform-format-type ${format} --data-schema ${DATA_SCHEMA}"
  local_xform_time_start=$(date --utc "+%s.%N")
  eval "$cmd3"
  local_xform_time_end=$(date --utc "+%s.%N")
  local_xform_time_dur=0$(echo "$local_xform_time_end - $local_xform_time_start" | bc)
  echo "Command ran: ${cmd3}" >> ${HOME}/local_xform_time_results.txt
  echo "local_xform_time_start=$local_xform_time_start local_xform_time_end=$local_xform_time_end local_xform_time_duration=$local_xform_time_dur" >> ${HOME}/local_xform_time_results.txt

  # merge using copy from append
  #cmd4="sudo bin/run-copyfrom-merge "
  cmd4="sudo bin/run-copyfrom-merge --pool ${poolname} --num-objs ${num_objs}"
  copyfromappend_merge_time_start=$(date --utc "+%s.%N")
  eval "$cmd4"
  copyfromappend_merge_time_end=$(date --utc "+%s.%N")
  copyfromappend_merge_time_dur=0$(echo "$copyfromappend_merge_time_end - $copyfromappend_merge_time_start" | bc)
  echo "Command ran: ${cmd4}" >> ${HOME}/copyfromappend_merge_time_results.txt
  echo "copyfromappend_merge_time_start=$copyfromappend_merge_time_start copyfromappend_merge_time_end=$copyfromappend_merge_time_end copyfromappend_merge_time_duration=$copyfromappend_merge_time_dur" >> ${HOME}/copyfromappend_merge_time_results.txt

  # merge using client
  #cmd5="sudo bin/run-merge"
  cmd5="sudo bin/run-client-merge --pool ${poolname} --num-objs ${num_objs}"
  clientmerge_time_start=$(date --utc "+%s.%N")
  eval "$cmd5"
  clientmerge_time_end=$(date --utc "+%s.%N")
  clientmerge_time_dur=0$(echo "$clientmerge_time_end - $clientmerge_time_start" | bc)
  echo "Command ran: ${cmd5}" >> ${HOME}/clientmerge_time_results.txt
  echo "clientmerge_time_start=$clientmerge_time_start clientmerge_time_end=$clientmerge_time_end clientmerge_time_duration=$clientmerge_time_dur" >> ${HOME}/clientmerge_time_results.txt

done
end=$(date --utc "+%s.%N")
dur=0$(echo "$end - $start" | bc)
echo "total runtime:"
echo "start=$start end=$end duration=$dur"
