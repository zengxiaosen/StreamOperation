#!/bin/bash
## replay webm to mp4, and upload to bos
## @param records_root the root path of records 
set -e

convert_webm_to_mp4()
{
    folder=${1}
    for filename in $(ls ${folder}/*.webm)  
    do
        webmfile=${filename}
        mp4file=${webmfile%.*}.mp4
	echo "convert_webm_to_mp4 ${webmfile}"
        echo ffmpeg -y -i ${webmfile} -strict -2 ${mp4file}
        ffmpeg -y -i ${webmfile} -strict -2 ${mp4file}
        rm ${webmfile}
    done
}

process_record()
{
    folder=${1}
    process_count=0
    for filename in $(ls ${folder})
    do
        ext=${filename##*.} 
        if [ "${ext}" == "pb" ]
        then
            let process_count=process_count+1
    	    # replay pb file
            pbfile=${folder}/${filename}
	    ./replay_pipeline_main \
	    	--logtostderr \
	    	--replay_files=${pbfile} \
	    	--export_directory=${folder}/ \
	    	--record_format=webm \
	    	--use_ntp_time=true \
	    	--debug_console=false
    
    	    # convert to mp4
    	    convert_webm_to_mp4 ${folder}
            # add finish flag
            # clear
            rm ${pbfile}
        fi
    done 
    
    echo "process count: ${process_count}"
    if [ ${process_count} -gt "0" ]
    then
        touch ${folder}/finish.tag
    fi
}

## main 
records_root=${1}
for folder in $(ls ${records_root})
do
   record_folder=${records_root}/$folder 
   echo process_record ${record_folder} 
   process_record ${record_folder} 
done

## upload
cd upload
bos_bucket=testvipkid
python run_upload_records.py ${records_root} ${bos_bucket} 
cd -

echo "success"

