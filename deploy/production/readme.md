## how to start monitor manually
* move monitor.sh and make_statuz.sh to /home/ in docker 
```
cp monitor.sh make_statuz.sh /home/
```
* make olive and orbit server ready to start
* start monitor.sh
```
cd /home/
nohup ./monitor.sh >> log.txt 2>&1 &
```
* start statuz page cache webservice when crash 
```
cd /home/statusz_htmls
nohup  python -m SimpleHTTPServer 12345 &
```

