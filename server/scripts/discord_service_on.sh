SVC_FILE=S92status

if [ -f /etc/init.d/$SVC_FILE ]; then
    exit 0
fi

if [ -f /etc/init.d/_$SVC_FILE ]; then
    mv /etc/init.d/_$SVC_FILE /etc/init.d/$SVC_FILE
    /etc/init.d/$SVC_FILE start
else
    echo "Discord status service not found, please run update_discord_status.sh."
    exit 1
fi

