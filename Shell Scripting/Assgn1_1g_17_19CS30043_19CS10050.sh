shuf -r -i1-10000000 -n1500|sed -z 's/\n/,/g'|sed 's/,/\n/10;P;D'>$1;cat $1|cut -d',' -f$2|grep -q $3&&echo "YES"||echo "NO"