/*
 * CredaCash (TM) cryptocurrency and blockchain
 *
 * Copyright (C) 2015-2016 Creda Software, Inc.
 *
 * service.cpp
*/

#include "CCdef.h"
#include "service.hpp"
#include "server.hpp"

#include <CCthread.hpp>
#include <boost/bind.hpp>

#include <iostream>

using namespace std;

CCThreadFactory CCDefaultThreadFac;

namespace CCServer {

void Service::Start(const boost::asio::ip::tcp::endpoint& endpoint, unsigned nthreads, unsigned maxconns, unsigned maxincoming, unsigned backlog, const class ConnectionFactory &connfac, const class CCThreadFactory &threadfac)
{
	BOOST_LOG_TRIVIAL(info) << Name() << " Service::Init nthreads " << nthreads << " maxconns " << maxconns << " maxincoming " << maxincoming << " backlog " << backlog;

	if (maxconns < 1)
	{
		BOOST_LOG_TRIVIAL(info) << Name() << " Service::Init nthreads skipping service since maxconns < 1";

		return;
	}

	// this was designed to that a number of servers can listen to the same port using SO_REUSEPORT
	// the problem is the OS can send all connections to a single server, so it is only useful if all servers have multiple threads and connections
	// so for now, we just use one server

	const unsigned nservers = 1;

	unsigned threads_per_server = (nthreads + nservers - 1)/nservers;
	unsigned connections_per_server = (maxconns + nservers - 1)/nservers;
	unsigned incoming_connections_per_server = (maxincoming + nservers - 1)/nservers;

	if (threads_per_server < 20)
		threads_per_server = 20;	// make sure we have enough threads to shutdown

	if (connections_per_server < 1)
		connections_per_server = 1;

	m_servers.reserve(nservers);
	m_threads.reserve(threads_per_server * nservers);

	for (unsigned i = 0; i < nservers; ++i)
	{
		auto s = new Server(Name());
		m_servers.push_back(s);

		s->Init(endpoint, connections_per_server, incoming_connections_per_server, backlog, connfac);

		for (unsigned j = 0; j < threads_per_server && !g_shutdown; ++j)
		{
			auto t = threadfac.NewThread();
			t->Run(boost::bind(&Server::Run, s));
			m_threads.push_back(t);
		}
	}
}

void Service::WaitForShutdown()
{
	for (auto t : m_threads)
	{
		t->join();
		delete t;
	}

	for (auto s : m_servers)
		delete s;

	m_threads.clear();
	m_servers.clear();
}

} // namespace CCServer
