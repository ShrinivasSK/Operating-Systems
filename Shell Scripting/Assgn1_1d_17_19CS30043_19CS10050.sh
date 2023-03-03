mkdir -p files_mod;for file in $(ls data1d/temp/);do
    sed 's/ /,/g' data1d/temp/$file|nl -s','>files_mod/$file;done;