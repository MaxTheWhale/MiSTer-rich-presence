SVC_FILE=S92status

if [ -f /etc/init.d/$SVC_FILE ]; then
    /etc/init.d/$SVC_FILE stop
    mv /etc/init.d/$SVC_FILE /etc/init.d/_$SVC_FILE
fi

