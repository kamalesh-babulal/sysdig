//
// socket_collector.h
//

#pragma once

#ifdef HAS_CAPTURE

#include "socket_handler.h"

template <typename T>
class socket_collector
{
public:
	typedef std::map<int, std::shared_ptr<T>> socket_map_t;

	socket_collector(bool do_loop = false, long timeout_ms = 1000L):
		m_nfds(0),
		m_loop(do_loop),
		m_timeout_ms(timeout_ms),
		m_stopped(false)
	{
		clear();
	}

	~socket_collector()
	{
	}

	void add(std::shared_ptr<T> handler)
	{
		if(handler)
		{
			int sockfd = handler->get_socket(m_timeout_ms);
			m_sockets[sockfd] = handler;
			g_logger.log("Socket collector: handler [" + handler->get_id() +
						 "] added socket (" + std::to_string(sockfd) + ')',
						 sinsp_logger::SEV_TRACE);
		}
		else
		{
			g_logger.log("Socket collector: atempt to add null handler.",
						 sinsp_logger::SEV_ERROR);
		}
	}

	bool is_enabled(std::shared_ptr<T> handler) const
	{
		if(handler)
		{
			return handler->is_enabled();
		}
		return false;
	}

	void enable(std::shared_ptr<T> handler)
	{
		if(handler)
		{
			handler->enable();
			return;
		}
		g_logger.log("Socket collector: attempt to enable non-existing handler: " + handler->get_id());
	}

	int get_socket(std::shared_ptr<T> handler) const
	{
		for(const auto& http : m_sockets)
		{
			if(http.second == handler)
			{
				return http.first;
			}
		}
		return -1;
	}

	bool has(std::shared_ptr<T> handler)
	{
		for(const auto& http : m_sockets)
		{
			if(http.second == handler)
			{
				return true;
			}
		}
		return false;
	}

	bool remove(std::shared_ptr<T> handler)
	{
		for(typename socket_map_t::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it)
		{
			if(it->second == handler)
			{
				remove(it);
				return true;
			}
		}
		return false;
	}

	void remove_all()
	{
		clear();
		m_sockets.clear();
		m_nfds = 0;
	}

	int subscription_count() const
	{
		return m_sockets.size();
	}

	int signaled_sockets_count()
	{
		int count = 0;
		for(typename socket_map_t::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it)
		{
			if(FD_ISSET(it->first, &m_infd))
			{
				++count;
			}
		}
		return count;
	}

	void trace_sockets()
	{
		if(g_logger.get_severity() >= sinsp_logger::SEV_TRACE)
		{
			for(typename socket_map_t::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it)
			{
				if(it->second)
				{
					if(it->second->is_enabled())
					{
						g_logger.log("Socket collector: examining socket " + std::to_string(it->first) +
										 " (" + it->second->get_id() + ')', sinsp_logger::SEV_TRACE);
						if(FD_ISSET(it->first, &m_infd))
						{
							g_logger.log("Socket collector: activity on socket " + std::to_string(it->first) +
										 " (" + it->second->get_id() + ')', sinsp_logger::SEV_TRACE);
						}
					}
					else
					{
						g_logger.log("Socket collector: socket " + std::to_string(it->first) +
									 " handler (" + it->second->get_id() + ") is not enabled.",
									 sinsp_logger::SEV_TRACE);
					}
				}
				else
				{
					g_logger.log("Socket collector: socket " + std::to_string(it->first) +
								 " handler (" + it->second->get_id() + ") is null.",
								 sinsp_logger::SEV_TRACE);
				}
			}
		}
	}

	void enable_sockets()
	{
		for(typename socket_map_t::iterator it = m_sockets.begin(); it != m_sockets.end(); ++it)
		{
			int sockfd = -1;
			if(it->second)
			{
				if(it->second->is_enabled())
				{
					sockfd = it->first;
					FD_SET(sockfd, &m_infd);
					FD_SET(sockfd, &m_errfd);
					if(sockfd > m_nfds)
					{
						m_nfds = sockfd;
					}
				}
			}
		}
	}

	void get_data()
	{
		try
		{
			struct timeval tv;
			int res;
			m_stopped = false;
			while(!m_stopped)
			{
				tv.tv_sec  = m_loop ? m_timeout_ms / 1000 : 0;
				tv.tv_usec = m_loop ? (m_timeout_ms % 1000) * 1000 : 0;
				{
					if(m_sockets.size())
					{
						enable_sockets(); // flag all enabled handler sockets
						g_logger.log("Socket collector: total sockets=" + std::to_string(m_sockets.size()) +
										 ", select-enabled sockets= " + std::to_string(signaled_sockets_count()),
										 sinsp_logger::SEV_TRACE);
						res = select(m_nfds + 1, &m_infd, NULL, &m_errfd, &tv);
						g_logger.log("Socket collector: total sockets=" + std::to_string(m_sockets.size()) +
										 ", signaled sockets= " + std::to_string(signaled_sockets_count()),
										 sinsp_logger::SEV_TRACE);
						if(res == 0) // all quiet
						{
							g_logger.log("Socket collector: " + std::to_string(m_sockets.size()) + " sockets total, no activity.",
										 sinsp_logger::SEV_DEBUG);
						}
						else if(res < 0) // select error
						{
							// socket sets are undefined after error, nothing to do here ...
							throw sinsp_exception(std::string("Socket collector: select error (").append(strerror(errno)).append(1, ')'));
						}
						else // data available or socket error
						{
							trace_sockets();
							for(typename socket_map_t::iterator it = m_sockets.begin(); it != m_sockets.end();)
							{
								std::string id = it->second->get_id();
								if(FD_ISSET(it->first, &m_infd))
								{
									if(it->second && !it->second->on_data())
									{
										if(errno != EAGAIN)
										{
											g_logger.log("Socket collector data handling error " + std::to_string(errno) + ", (" +
														 strerror(errno) + "), removing handler " +
														 (it->second ? it->second->get_id() : "Unknown"),
														 sinsp_logger::SEV_ERROR);
											remove(it);
											continue;
										}
									}
								}

								if(FD_ISSET(it->first, &m_errfd))
								{
									if(errno != EAGAIN)
									{
										std::string err = strerror(errno);
										g_logger.log(err, sinsp_logger::SEV_ERROR);
										std::string fid;
										if(it->second)
										{
											it->second->on_error(err, true);
											g_logger.log("Socket collector: socket error, removing handler [" + id + "], "
														 "socket " + std::to_string(it->first), sinsp_logger::SEV_ERROR);
										}
										remove(it);
										continue;
									}
								}
								++it;
							}
						}
					}
					else
					{
						g_logger.log("Socket collector is empty. Stopping.", sinsp_logger::SEV_ERROR);
						m_stopped = true;
						return;
					}
				}
				if(!m_loop) { break; }
			}
		}
		catch(std::exception& ex)
		{
			g_logger.log(std::string("Socket collector error: ") + ex.what(), sinsp_logger::SEV_ERROR);
			remove_all();
			m_stopped = true;
		}
	}

	void stop()
	{
		m_stopped = true;
	}

	bool is_active() const
	{
		return subscription_count() > 0;
	}

	bool is_healthy(std::shared_ptr<T> handler) const
	{
		return get_socket(handler) != -1;
	}

private:
	void clear()
	{
		FD_ZERO(&m_errfd);
		FD_ZERO(&m_infd);
	}

	typename socket_map_t::iterator& remove(typename socket_map_t::iterator& it)
	{
		if(it != m_sockets.end())
		{
			m_sockets.erase(it++);
		}
		m_nfds = 0;
		for(const auto& sock : m_sockets)
		{
			if(sock.first > m_nfds)
			{
				m_nfds = sock.first;
			}
		}
		return it;
	}

	socket_map_t     m_sockets;
	fd_set           m_infd;
	fd_set           m_errfd;
	int              m_nfds;
	bool             m_loop;
	long             m_timeout_ms;
	bool             m_stopped;
};

#endif // HAS_CAPTURE
