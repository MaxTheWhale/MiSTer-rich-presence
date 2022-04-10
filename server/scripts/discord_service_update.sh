RELEASE_URL=https://github.com/MaxTheWhale/MiSTer-rich-presence/releases/download/v0.1/mister_status_v0.1.zip
OUTPUT_FILE=release.zip
BIN_FILE=mister_status
SVC_FILE=S92status

echo "Fetching latest release..."
curl -L -o $OUTPUT_FILE $RELEASE_URL

echo "Extracting files..."
unzip -o $OUTPUT_FILE $BIN_FILE $SVC_FILE
chmod +x $BIN_FILE $SVC_FILE
rm $OUTPUT_FILE

echo "Copying files..."
mv $BIN_FILE /sbin

if [ -f /etc/init.d/$SVC_FILE ]; then
    mv $SVC_FILE /etc/init.d
    /etc/init.d/$SVC_FILE restart
else 
    mv $SVC_FILE /etc/init.d/_$SVC_FILE
fi

echo "Discord status service is installed and up-to-date."



