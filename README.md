# natnf
Export information about nat translates
## Authors
Jakub Mackovič, Jakub Pastuszek
VUT FIT Brno

### Debug
- File **utils.c** constains variable to enable debug msgs named *is_debug*.
- File **export.c** contains initialize call for syslog *msg_init()* which with true parameter print out syslog msgs.

## Usage
```
natnf [ -h ] [ -c <collector-ip-address> ]  [ -p <collector-port> ] [ -s ] [ -l <syslog-level> ] [ -t <template-timeout> ] [ -e <export-timeout> ] [ -d <log-path> ] [ -f ]
	-h help
	-c collector IP address
	-p collector port
	-s enable syslog logging
	-l syslog level
	-t template timeout [seconds]
	-e export timeout [seconds]
	-d log path
	-f daemonize
```

