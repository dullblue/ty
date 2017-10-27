绑定2张网卡
ifconfig eth0:0 192.168.5.110 netmask 255.255.255.0 up
ifconfig eth0:1 192.168.5.111 netmask 255.255.255.0 up


ulimit -u 1124000	-u  The maximum number of processes available to a single user				
ulimit -n 1124000	-n  The maximum number of open file descriptors (most systems do not allow this value to be set)
echo "1024" >  /proc/sys/net/core/somaxconn		listen队列
echo "1048576" > /proc/sys/fs/file-max			系统限制单个进程打开文件输的限制

cat /proc/sys/fs/file-nr		查看：可以看到整个系统目前使用的文件句柄数量(当前文件数，浪费数，最大数)	129900  0       1048576
			


vi /etc/security/limits.conf	增加
* soft nofile 1048576
* hard nofile 1048576

cat /proc/sys/fs/file-max		1048576

vi /etc/sysctl.conf
fs.file-max = 1048576
net.core.somaxconn = 2048  
net.ipv4.tcp_rmem = 4096 4096 16777216  
net.ipv4.tcp_wmem = 4096 4096 16777216  
net.ipv4.tcp_mem = 786432 2097152 3145728  
net.ipv4.tcp_max_syn_backlog = 16384  
net.core.netdev_max_backlog = 20000  
net.ipv4.tcp_fin_timeout = 15  
net.ipv4.tcp_max_syn_backlog = 16384  
net.ipv4.tcp_tw_reuse = 1  
net.ipv4.tcp_tw_recycle = 1  
net.ipv4.tcp_max_orphans = 131072  

//can be or can not be
net.core.rmem_default = 262144  
net.core.wmem_default = 262144  
net.core.rmem_max = 16777216  
net.core.wmem_max = 16777216 
 
//no need to set
net.ipv4.ip_conntrack_max = 1048576				这表明将系统对最大跟踪的TCP连接数限制设置  请注意，此限制值要尽量小，以节省对内核内存的占用。
net.ipv4.netfilter.ip_conntrack_max = 1048576


/sbin/sysctl -p 生效


cat /proc/net/sockstat			查看：socket连接数量
