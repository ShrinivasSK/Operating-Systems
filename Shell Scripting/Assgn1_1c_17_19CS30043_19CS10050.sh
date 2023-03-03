ech=$(find ./data1c/data -type f|perl -ne 'print $1 if m/\.([^.\/]+)$/'|sort -u)
for t in $ech;do
    mkdir -p "./data1c/$t";names=($(find ./data1c/data -type f -name "*.$t"));for((i=0;i<${#names[*]};i+=1000));do
        mv ${names[@]:i:1000} ./data1c/$t;done;done;
mkdir -p "./data1c/NIL";names=($(find ./data1c/data -type f));for((i=0;i<${#names[*]};i+=1000));do
    mv ${names[@]:i:1000} ./data1c/NIL;done;rm -r data1c/data;