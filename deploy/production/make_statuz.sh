#!/bin/bash

#newname=statusz_$(date +"%Y%m%d%H%M%S").html
newname=$1.html

mkdir -p statusz_htmls
cd statusz_htmls
wget https://orbitlive.intviu.cn:11000/statusz
mv statusz ${newname} 
sed -i "s/\/file?file=Chart.min.js/https:\/\/cdn.bootcss.com\/Chart.js\/1.0.1\/Chart.min.js/g" ${newname}


