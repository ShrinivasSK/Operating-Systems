VERBOSE=$1
function log(){
	[ "$VERBOSE" == "-v" ]&&printf "\n$@\n\n"
}
export REQ_HEADERS="Accept,Host"
log "Saving Example.com as HTML File."
curl https://www.example.com/>example.html;
log "Saved file Successfully."
log "Getting IP and Response Headers"
curl -D - http://ip.jsontest.com/;
log "Parsing JSON response. Ouput: "
resp=$(curl http://headers.jsontest.com/)
for header in ${REQ_HEADERS//,/ };do
    echo -n "$header:";echo $resp|jq ".$header";
done;
log "Checking for validity of JSON files"
for file in $(ls JSONData/);do
    resp=$(curl -d "json=`cat JSONData/$file`" -X POST http://validate.jsontest.com/);
    $(echo $resp|jq ".validate")&&echo $file>>valid.txt||echo $file>>invalid.txt;
done;
log "Done."