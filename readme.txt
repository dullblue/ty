��2������
ifconfig eth0:0 192.168.5.110 netmask 255.255.255.0 up
ifconfig eth0:1 192.168.5.111 netmask 255.255.255.0 up


ulimit -u 1124000	-u  The maximum number of processes available to a single user				
ulimit -n 1124000	-n  The maximum number of open file descriptors (most systems do not allow this value to be set)
echo "1024" >  /proc/sys/net/core/somaxconn		listen����
echo "1048576" > /proc/sys/fs/file-max			ϵͳ���Ƶ������̴��ļ��������

cat /proc/sys/fs/file-nr		�鿴�����Կ�������ϵͳĿǰʹ�õ��ļ��������(��ǰ�ļ������˷����������)	129900  0       1048576
			


vi /etc/security/limits.conf	����
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
net.ipv4.ip_conntrack_max = 1048576				�������ϵͳ�������ٵ�TCP��������������  ��ע�⣬������ֵҪ����С���Խ�ʡ���ں��ڴ��ռ�á�
net.ipv4.netfilter.ip_conntrack_max = 1048576


/sbin/sysctl -p ��Ч


cat /proc/net/sockstat			�鿴��socket��������
