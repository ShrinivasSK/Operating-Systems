mkdir -p 1.b.files.out;for entry in `ls 1.b.files`;do sort -n 1.b.files/$entry>1.b.files.out/$entry;done;
cat 1.b.files/*.txt|sort -n|uniq -c|awk '{print $2,$1}'>1.b.out.txt
