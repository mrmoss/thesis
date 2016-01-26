//Tested on:
//	OSX 10.11.2 (g++)

//TODO: Check memory allocs and frees, cleanup, sort tcp and udp output, make errors better.

#include "netstat.hpp"

#include <sys/sysctl.h>
#include <iostream>
#include <libproc.h>

#include "netstat_util.hpp"
#include "string_util.hpp"

static const size_t states_size=14;
static const std::string states[states_size]=
{
	"CLOSED",
	"LISTEN",
	"SYN_SENT",
	"SYN_RECV",
	"ESTABLISHED",
	"CLOSE_WAIT",
	"FIN_WAIT1",
	"CLOSING",
	"LAST_ACK",
	"FIN_WAIT2",
	"TIME_WAIT",
	"RESERVED"
};

netstat_list_t netstat()
{
	netstat_list_t netstats;

	int name[]={CTL_KERN,KERN_PROC,KERN_PROC_ALL};
	size_t num_procs=0;

	if(sysctl(name,3,NULL,&num_procs,NULL,0)!=0)
		throw std::runtime_error("ERROR 0");

	kinfo_proc* procs=(kinfo_proc*)malloc(num_procs);
	if(procs==NULL)
		throw std::runtime_error("ERROR 0");

	if(sysctl(name,3,procs,&num_procs,NULL,0)!=0)
	{
		free(procs);
		throw std::runtime_error("ERROR 0");
	}

	num_procs=num_procs/sizeof(struct kinfo_proc);

	for(size_t ii=0;ii<num_procs;++ii)
	{
		int num_fds=proc_pidinfo(procs[ii].kp_proc.p_pid,PROC_PIDLISTFDS,0,NULL,0);

		if(num_fds<=0)
			continue;

		proc_fdinfo* fds=(proc_fdinfo*)malloc(num_fds);
		if(fds==NULL)
			continue;

		num_fds=proc_pidinfo(procs[ii].kp_proc.p_pid,PROC_PIDLISTFDS,0,fds,num_fds);
		if(num_fds<0)
		{
			free(fds);
			continue;
		}

		num_fds=num_fds/PROC_PIDLISTFD_SIZE;

		for(size_t jj=0;jj<num_fds;++jj)
		{
			if(fds[jj].proc_fdtype==PROX_FDTYPE_SOCKET)
			{
				socket_fdinfo si;

				if(proc_pidfdinfo(procs[ii].kp_proc.p_pid,fds[jj].proc_fd,PROC_PIDFDSOCKETINFO,&si,sizeof(si))<=0)
					continue;

				bool is_inet=(si.psi.soi_family==AF_INET);
				bool is_ipv6=false;

				#if(defined(AF_INET6))
					if(si.psi.soi_family==AF_INET6)
					{
						is_inet=true;
						is_ipv6=true;
					}
				#endif

				if(is_inet)
				{
						netstat_t netstat;

						netstat.pid=std::to_string(procs[ii].kp_proc.p_pid);
						if(si.psi.soi_type==SOCK_STREAM)
						{
							netstat.proto="tcp";
							netstat.state=states[si.psi.soi_proto.pri_tcp.tcpsi_state];
						}

						else if(si.psi.soi_type==SOCK_DGRAM)
							netstat.proto="udp";

						if(netstat.proto.size()>0)
						{
							if(is_ipv6)
							{
								netstat.proto+="6";
								netstat.local_address=uint8_t_16_to_ipv6((uint8_t*)&si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_6);
								netstat.foreign_address=uint8_t_16_to_ipv6((uint8_t*)&si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_6);
							}
							else
							{
								netstat.proto+="4";
								netstat.local_address=uint32_t_to_ipv4(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_laddr.ina_46.i46a_addr4.s_addr);
								netstat.foreign_address=uint32_t_to_ipv4(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_faddr.ina_46.i46a_addr4.s_addr);
							}

							if(si.psi.soi_type==SOCK_DGRAM)
							{
								netstat.local_port=uint16_t_to_port(si.psi.soi_proto.pri_in.insi_lport);
								netstat.foreign_port=uint16_t_to_port(si.psi.soi_proto.pri_in.insi_fport);
							}
							else
							{
								netstat.local_port=uint16_t_to_port(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport);
								netstat.foreign_port=uint16_t_to_port(si.psi.soi_proto.pri_tcp.tcpsi_ini.insi_fport);
							}

							netstats.push_back(netstat);
						}
				}
			}
		}

		free(fds);
	}

	free(procs);

	return netstats;
}