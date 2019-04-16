# Remove cbl-log directory
if [ -d "/opt/cbl-log" ]; then
    rm -rf /opt/cbl-log  > /dev/null 2>&1
fi
exit 0 
