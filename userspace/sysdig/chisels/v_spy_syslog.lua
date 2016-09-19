--[[
Copyright (C) 2013-2015 Draios inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]

view_info = 
{
	id = "spy_syslog",
	name = "Spy Syslog",
	description = "Show the entries written to syslog.",
	tips = {"This view can be applied to the whole system, to watch overall syslog activity, but is also useful when applied to a container or a process. In that case, the view will only show the syslog writes generated by the selected entity."},
	tags = {"Default"},
	view_type = "list",
	applies_to = {"", "container.id", "proc.pid", "proc.name", "thread.tid", "fd.sport", "fd.sproto", "k8s.pod.id", "k8s.rc.id", "k8s.rs.id", "k8s.svc.id", "k8s.ns.id", "marathon.app.id", "marathon.group.name", "mesos.task.id", "mesos.framework.name"},
	filter = "fd.name contains /dev/log and evt.is_io_write=true and evt.dir=< and evt.failed=false",
	columns = 
	{
		{
			name = "PID",
			field = "proc.pid",
			description = "PID of the process generating the message.",
			colsize = 8,
		},
		{
			name = "PROC",
			field = "proc.name",
			description = "Name of the process generating the message.",
			colsize = 8,
		},
		{
			name = "FAC",
			field = "syslog.facility.str",
			description = "syslog facility of the message.",
			colsize = 8,
		},
		{
			name = "SEV",
			field = "syslog.severity.str",
			description = "syslog severity of the message.",
			colsize = 8,
		},
		{
			tags = {"containers"},
			name = "Container",
			field = "container.name",
			colsize = 20
		},
		{
			name = "MESSAGE",
			field = "syslog.message",
			description = "Message sent to syslog",
			colsize = 0
		}
	}
}
