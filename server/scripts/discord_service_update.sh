RELEASE_URL=https://github.com/MaxTheWhale/MiSTer-rich-presence/releases/latest/download/server.zip
OUTPUT_FILE=release.zip

BIN_FILE=mister-status
SVC_FILE=S92status
SCRIPTS_DIR=scripts

SCRIPTS_OUTPUT_DIR=/media/fat/Scripts

echo "Fetching latest release..."
curl -L -o $OUTPUT_FILE $RELEASE_URL

echo "Extracting files..."
unzip -o $OUTPUT_FILE
chmod +x $BIN_FILE $SVC_FILE

echo "Copying files..."
mv $BIN_FILE /sbin
cp $SCRIPTS_DIR/*.sh $SCRIPTS_OUTPUT_DIR

if [ -f /etc/init.d/$SVC_FILE ]; then
    mv $SVC_FILE /etc/init.d
    /etc/init.d/$SVC_FILE restart
else 
    mv $SVC_FILE /etc/init.d/_$SVC_FILE
fi

rm $OUTPUT_FILE
rm -r $SCRIPTS_DIR

echo "Discord status service is installed and up-to-date."



