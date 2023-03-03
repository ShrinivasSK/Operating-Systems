x=$1;for((i=2;i<=$x;i++));do
  if((!($x%$i)));then
    x=$((x/i));echo -n "$i ";i=$((i-1));fi;done;